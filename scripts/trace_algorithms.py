#!/usr/bin/env python3
"""Independent Python re-implementations of the C++ solvers, instrumented to print search traces.

Purpose: every worked trace in ALGORITHMS.md is generated here, and `--check` asserts that the
re-implementation reports the same node count and the same solution(s) as the real C++ CLI on the
same fixture.  If the C++ search order ever changes, `--check` fails and the document must be regenerated.

Usage:
  python3 scripts/trace_algorithms.py trace <game> <fixture>     # print the trace
  python3 scripts/trace_algorithms.py check                      # compare against ./build_release/solvedin (or $PUZZLE_CLI)
"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.environ.get("PUZZLE_CLI", os.path.join(ROOT, "build_release", "solvedin"))
LEX = os.path.join(ROOT, "fixtures", "default_lexicon.txt")


def _pr(m, s=None):
    print(m)


def lines_of(path):
    return [l.rstrip("\n") for l in open(path) if l.strip() and not l.lstrip().startswith("#")]


# --------------------------------------------------------------------------- Queens (bitset engine)
def queens(path, check_unique=True, log=_pr):
    L = lines_of(path)
    n = int(L[0])
    reg = [[int(t) for t in L[1 + r].split()] for r in range(n)]
    labels = sorted({x for row in reg for x in row})
    ridx = {v: i for i, v in enumerate(labels)}
    cell = lambda r, c: r * n + c
    row_m = [sum(1 << cell(r, c) for c in range(n)) for r in range(n)]
    col_m = [sum(1 << cell(r, c) for r in range(n)) for c in range(n)]
    reg_m = [0] * n
    reg_of = {}
    for r in range(n):
        for c in range(n):
            k = ridx[reg[r][c]]
            reg_of[cell(r, c)] = k
            reg_m[k] |= 1 << cell(r, c)
    blocked = {}
    for r in range(n):
        for c in range(n):
            m = row_m[r] | col_m[c] | reg_m[reg_of[cell(r, c)]]
            for dr in (-1, 0, 1):
                for dc in (-1, 0, 1):
                    if 0 <= r + dr < n and 0 <= c + dc < n:
                        m |= 1 << cell(r + dr, c + dc)
            blocked[cell(r, c)] = m
    pc = lambda x: bin(x).count("1")
    st = dict(nodes=0, sols=[], un=[set(range(n)), set(range(n)), set(range(n))], q=[])
    vis = {"hl": [], "x": []}
    _raw = log
    def log(msg):
        _raw(msg.strip(), {"q": [list(p) for p in st["q"]], "hl": vis["hl"], "x": vis["x"]})
    max_sol = 2 if check_unique else 1

    def wipeout(avail):
        for k in range(3):
            masks = (row_m, col_m, reg_m)[k]
            for u in sorted(st["un"][k]):
                if not (avail & masks[u]):
                    return True
        return False

    def rec(avail, depth):
        ind = "  " * depth
        st["nodes"] += 1
        node = st["nodes"]
        if len(st["sols"]) >= max_sol:
            return
        if len(st["q"]) == n:
            st["sols"].append(sorted(st["q"]))
            log(f"{ind}[node {node}] all {n} queens placed -> SOLUTION {st['sols'][-1]}")
            return
        best = None
        names = ("row", "col", "region")
        for k in range(3):
            masks = (row_m, col_m, reg_m)[k]
            for u in range(n):
                if u in st["un"][k]:
                    cnt = pc(avail & masks[u])
                    if cnt == 0:
                        log(f"{ind}[node {node}] WIPEOUT: {names[k]} {u} has 0 legal cells -> backtrack")
                        return
                    if best is None or cnt < best[0]:
                        best = (cnt, k, u)
        cnt, k, u = best
        vis["hl"] = []; vis["x"] = []
        unit = (row_m, col_m, reg_m)[k][u]
        legal = avail & unit
        cands = []
        pruned = []
        for ce in range(n * n):
            if legal >> ce & 1:
                r, c = divmod(ce, n)
                for kk, v in enumerate((r, c, reg_of[ce])):
                    st["un"][kk].discard(v)
                wo = wipeout(avail & ~blocked[ce])
                for kk, v in enumerate((r, c, reg_of[ce])):
                    st["un"][kk].add(v)
                if wo:
                    pruned.append((r, c))
                else:
                    cands.append((pc(avail & blocked[ce]), ce))
        cands.sort(key=lambda t: t[0])  # stable; equal to libc++ for the tiny lists traced here
        vis["hl"] = [list(divmod(ce, n)) for _, ce in cands]; vis["x"] = [list(p) for p in pruned]
        log(f"{ind}[node {node}] MRV picks {names[k]} {u} ({cnt} legal cells); "
            f"forward-check prunes {pruned if pruned else 'none'}; LCV order "
            f"{[(divmod(ce, n), s) for s, ce in cands]}")
        for s, ce in cands:
            if len(st["sols"]) >= max_sol:
                return
            r, c = divmod(ce, n)
            st["q"].append((r, c))
            for kk, v in enumerate((r, c, reg_of[ce])):
                st["un"][kk].discard(v)
            log(f"{ind}  place queen ({r},{c})")
            rec(avail & ~blocked[ce], depth + 1)
            for kk, v in enumerate((r, c, reg_of[ce])):
                st["un"][kk].add(v)
            st["q"].pop()
            vis["hl"] = []; vis["x"] = []
            log(f"{ind}  undo queen ({r},{c})")

    rec((1 << (n * n)) - 1, 0)
    return st["nodes"], st["sols"]


# --------------------------------------------------------------------------- Mini Sudoku
def sudoku(path, check_unique=True, log=_pr):
    L = lines_of(path)
    N = 6
    g0 = [[0 if t == "." else int(t) for t in L[1 + r].split()] for r in range(N)]
    box = lambda r, c: (r // 2) * 2 + c // 3
    peers = {}
    for r in range(N):
        for c in range(N):
            s = {(r, x) for x in range(N)} | {(x, c) for x in range(N)}
            b = box(r, c)
            br, bc = (b // 2) * 2, (b % 2) * 3
            s |= {(br + i, bc + j) for i in range(2) for j in range(3)}
            s.discard((r, c))
            peers[(r, c)] = sorted(s)
    ALL = 0b01111110
    SV = {"g": g0, "hl": None}
    _raw = log
    def log(msg):
        _raw(msg.strip(), {"g": "".join(str(x) for row in SV["g"] for x in row), "hl": SV["hl"]})
    pc = lambda x: bin(x).count("1")

    def assign(g, cd, r, c, d, why, depth):
        g[r][c] = d
        cd[r][c] = 1 << d
        SV["hl"] = [r, c]
        log("  " * depth + f"    {why}: ({r},{c}) = {d}")
        for (pr, pc_) in peers[(r, c)]:
            cd[pr][pc_] &= ~(1 << d)
            if cd[pr][pc_] == 0:
                return False
        return True

    def propagate(g, cd, depth):
        SV["g"] = g
        changed = True
        while changed:
            changed = False
            for r in range(N):
                for c in range(N):
                    if g[r][c] == 0:
                        k = pc(cd[r][c])
                        if k == 0:
                            return False
                        if k == 1:
                            d = cd[r][c].bit_length() - 1
                            g[r][c] = d
                            SV["hl"] = [r, c]
                            log("  " * depth + f"    naked single: ({r},{c}) = {d}")
                            for (pr, pc_) in peers[(r, c)]:
                                cd[pr][pc_] &= ~(1 << d)
                                if cd[pr][pc_] == 0:
                                    return False
                            changed = True
            units = ([("row", [(r, c) for c in range(N)]) for r in range(N)],
                     [("col", [(r, c) for r in range(N)]) for c in range(N)],
                     [("box", [((b // 2) * 2 + i, (b % 2) * 3 + j) for i in range(2) for j in range(3)]) for b in range(6)])
            for fam in units:
                for idx, (nm, cells) in enumerate(fam):
                    for d in range(1, N + 1):
                        where = [(r, c) for (r, c) in cells if cd[r][c] >> d & 1]
                        if not where:
                            return False
                        if len(where) == 1 and g[where[0][0]][where[0][1]] == 0:
                            r, c = where[0]
                            if not assign(g, cd, r, c, d, f"hidden single in {nm} {idx}", depth):
                                return False
                            changed = True
        return True

    st = dict(nodes=0, sols=[])
    max_sol = 2 if check_unique else 1

    def rec(g, cd, depth):
        st["nodes"] += 1
        node = st["nodes"]
        SV["g"] = g; SV["hl"] = None
        if len(st["sols"]) >= max_sol:
            return
        best = None
        full = True
        for r in range(N):
            for c in range(N):
                if g[r][c] == 0:
                    full = False
                    k = pc(cd[r][c])
                    if best is None or k < best[0]:
                        best = (k, r, c)
        if full:
            st["sols"].append([row[:] for row in g])
            log("  " * depth + f"[node {node}] grid full -> SOLUTION")
            return
        k, r, c = best
        if k == 0:
            return
        digs = [d for d in range(1, N + 1) if cd[r][c] >> d & 1]
        SV["hl"] = [r, c]
        log("  " * depth + f"[node {node}] stalled; MRV picks ({r},{c}) with candidates {digs}")
        for d in digs:
            g2, cd2 = [row[:] for row in g], [row[:] for row in cd]
            g2[r][c] = d
            cd2[r][c] = 1 << d
            SV["g"] = g2; SV["hl"] = [r, c]
            log("  " * depth + f"  try ({r},{c}) = {d}")
            ok = all(not (cd2.__getitem__(pr).__setitem__(pc_, cd2[pr][pc_] & ~(1 << d)) or cd2[pr][pc_] == 0)
                     for (pr, pc_) in peers[(r, c)])
            if ok and propagate(g2, cd2, depth):
                rec(g2, cd2, depth + 1)
            else:
                log("  " * depth + f"    contradiction (a cell has no candidate) -> undo ({r},{c}) = {d}")
            if len(st["sols"]) >= max_sol:
                return

    g = [row[:] for row in g0]
    cd = [[(1 << g[r][c]) if g[r][c] else ALL for c in range(N)] for r in range(N)]
    ok = True
    for r in range(N):
        for c in range(N):
            if g[r][c]:
                for (pr, pc_) in peers[(r, c)]:
                    cd[pr][pc_] &= ~(1 << g[r][c])
                    ok &= cd[pr][pc_] != 0
    log("initial propagation from givens:")
    if ok and propagate(g, cd, 0):
        rec(g, cd, 0)
    return st["nodes"], st["sols"]


# --------------------------------------------------------------------------- Tango
S, M, B = 1, 2, 3


def tango(path, check_unique=True, log=_pr):
    L = lines_of(path)
    n = int(L[0])
    i = L.index("GRID")
    grid = [L[i + 1 + r].split() for r in range(n)]
    h = [L[L.index("HORIZONTAL") + 1 + r].split() for r in range(n)]
    v = [L[L.index("VERTICAL") + 1 + r].split() for r in range(n - 1)]
    T = n // 2
    cnt = lambda lst, m: sum(1 for x in lst if x == m)
    TV = {"d": None, "hl": None}
    _raw = log
    def log(msg):
        d_ = TV["d"]
        _raw(msg.strip(), {"g": "".join("S" if x == S else "M" if x == M else "." for row in d_ for x in row) if d_ else "", "hl": TV["hl"]})

    def prop(d, depth):
        TV["d"] = d
        ind = "  " * depth
        changed = True

        def setd(r, c, val, why):
            if d[r][c] != val:
                d[r][c] = val
                TV["hl"] = [r, c]
                log(f"{ind}    {why}: ({r},{c}) -> {'S' if val == S else 'M' if val == M else 'both'}")
                return True
            return False

        while changed:
            changed = False
            for (clues, dr, dc, R, C) in ((h, 0, 1, n, n - 1), (v, 1, 0, n - 1, n)):
                for r in range(R):
                    for c in range(C):
                        cl = clues[r][c]
                        a, b = d[r][c], d[r + dr][c + dc]
                        if cl == "=":
                            com = a & b
                            if com == 0:
                                return False
                            changed |= setd(r, c, com, "'=' edge") | setd(r + dr, c + dc, com, "'=' edge")
                        elif cl == "x":
                            for (p, q, pr, pc_, qr, qc) in ((a, b, r, c, r + dr, c + dc), (b, a, r + dr, c + dc, r, c)):
                                if p in (S, M):
                                    need = M if p == S else S
                                    if not (q & need):
                                        return False
                                    changed |= setd(qr, qc, need, "'x' edge")
                                a, b = d[r][c], d[r + dr][c + dc]
            for lines, name in (([[(r, c) for c in range(n)] for r in range(n)], "row"),
                                ([[(r, c) for r in range(n)] for c in range(n)], "col")):
                for ln in lines:
                    vals = [d[r][c] for r, c in ln]
                    s_, m_ = cnt(vals, S), cnt(vals, M)
                    if s_ > T or m_ > T:
                        return False
                    if s_ == T:
                        for r, c in ln:
                            if d[r][c] == B:
                                changed |= setd(r, c, M, f"{name} already has {T} Suns")
                    if m_ == T:
                        for r, c in ln:
                            if d[r][c] == B:
                                changed |= setd(r, c, S, f"{name} already has {T} Moons")
            for lines in ([[(r, c) for c in range(n)] for r in range(n)], [[(r, c) for r in range(n)] for c in range(n)]):
                for ln in lines:
                    for k in range(n - 2):
                        for sym in (S, M):
                            opp = M if sym == S else S
                            a, b, c3 = ln[k], ln[k + 1], ln[k + 2]
                            for (x, y, z) in ((a, b, c3), (b, c3, a), (a, c3, b)):
                                if d[x[0]][x[1]] == sym and d[y[0]][y[1]] == sym:
                                    if not (d[z[0]][z[1]] & opp):
                                        return False
                                    changed |= setd(z[0], z[1], opp, "no three in a row")
        return True

    st = dict(nodes=0, sols=[])
    max_sol = 2 if check_unique else 1

    def rec(d, depth):
        st["nodes"] += 1
        node = st["nodes"]
        TV["d"] = d; TV["hl"] = None
        if len(st["sols"]) >= max_sol:
            return
        pick = next(((r, c) for r in range(n) for c in range(n) if d[r][c] == B), None)
        if pick is None:
            st["sols"].append(["".join("S" if x == S else "M" for x in row) for row in d])
            log("  " * depth + f"[node {node}] every cell fixed -> SOLUTION")
            return
        r, c = pick
        TV["hl"] = [r, c]
        log("  " * depth + f"[node {node}] stalled; branch on first undecided cell (row-major) ({r},{c})")
        for sym in (S, M):
            d2 = [row[:] for row in d]
            d2[r][c] = sym
            TV["d"] = d2; TV["hl"] = [r, c]
            log("  " * depth + f"  try ({r},{c}) = {'S' if sym == S else 'M'}")
            if prop(d2, depth):
                rec(d2, depth + 1)
            else:
                log("  " * depth + "    contradiction -> undo")
            if len(st["sols"]) >= max_sol:
                return

    d = [[S if grid[r][c] == "S" else M if grid[r][c] == "M" else B for c in range(n)] for r in range(n)]
    log("initial propagation:")
    if prop(d, 0):
        rec(d, 0)
    return st["nodes"], st["sols"]


# --------------------------------------------------------------------------- Zip
def zip_(path, check_unique=True, log=_pr):
    L = lines_of(path)
    R, C = map(int, L[0].split())
    i = L.index("GRID")
    cp = [[0 if t == "." else int(t) for t in L[i + 1 + r].split()] for r in range(R)]
    wh = [[False] * C for _ in range(R)]  # wall between (r,c) and (r+1,c)
    wv = [[False] * C for _ in range(R)]  # wall between (r,c) and (r,c+1)
    if "WALLS_H" in L:
        j = L.index("WALLS_H")
        for r in range(R - 1):
            for c, t in enumerate(L[j + 1 + r].split()):
                wh[r][c] = t == "-"
    if "WALLS_V" in L:
        j = L.index("WALLS_V")
        for r in range(R):
            for c, t in enumerate(L[j + 1 + r].split()):
                wv[r][c] = t == "|"
    pos = {}
    for r in range(R):
        for c in range(C):
            if cp[r][c]:
                pos[cp[r][c]] = (r, c)
    K = max(pos)
    inb = lambda p: 0 <= p[0] < R and 0 <= p[1] < C

    def can(a, b):
        if abs(a[0] - b[0]) + abs(a[1] - b[1]) != 1 or not inb(a) or not inb(b):
            return False
        if a[0] != b[0]:
            return not wh[min(a[0], b[0])][a[1]]
        return not wv[a[0]][min(a[1], b[1])]

    D = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    visited = {pos[1]}
    path = [pos[1]]
    total = R * C
    end = pos[K]
    st = dict(nodes=0, sols=[], cut=0, deg=0, cp=0)
    ZV = {"flag": None, "moves": []}
    _raw = log
    def log(msg):
        _raw(msg.strip(), {"path": [list(q) for q in path], "flag": ZV["flag"], "moves": ZV["moves"]})
    max_sol = 2 if check_unique else 1

    def cuts(head, nxt_cp, remaining):
        if remaining <= 1:
            return None
        start = next(((head[0] + a, head[1] + b) for a, b in D
                      if inb((head[0] + a, head[1] + b)) and (head[0] + a, head[1] + b) not in visited
                      and can(head, (head[0] + a, head[1] + b))), None)
        if start is None:
            return "head has no unvisited neighbour"
        seen = {start}
        q = [start]
        for cur in q:
            for a, b in D:
                nx = (cur[0] + a, cur[1] + b)
                if inb(nx) and nx not in visited and can(cur, nx) and nx not in seen:
                    seen.add(nx)
                    q.append(nx)
        if len(seen) < remaining:
            return f"unvisited region split: BFS reaches {len(seen)} of {remaining} cells"
        if nxt_cp <= K and pos[nxt_cp] not in seen:
            return f"checkpoint {nxt_cp} unreachable"
        for r in range(R):
            for c in range(C):
                if (r, c) not in visited:
                    u = (r, c)
                    k = sum(1 for a, b in D if inb((r + a, c + b)) and (r + a, c + b) not in visited and can(u, (r + a, c + b)))
                    k += 1 if can(u, head) else 0
                    if u == end and k < 1:
                        return "end cell cannot be entered"
                    if u != end and k < 2:
                        return f"cell {u} would have only {k} free neighbour(s)"
        return None

    def dfs(nxt_cp, depth):
        ind = "  " * depth
        st["nodes"] += 1
        node = st["nodes"]
        if len(path) == total:
            if path[-1] == end and nxt_cp == K + 1:
                st["sols"].append(list(path))
                log(f"{ind}[node {node}] path covers all {total} cells and ends at checkpoint {K} -> SOLUTION")
            return
        cur = path[-1]
        why = cuts(cur, nxt_cp, total - len(path))
        if why:
            ZV["flag"] = "prune"; ZV["moves"] = []
            log(f"{ind}[node {node}] head {cur}: PRUNE ({why})")
            ZV["flag"] = None
            return
        cands = []
        for a, b in D:
            nx = (cur[0] + a, cur[1] + b)
            if not inb(nx) or nx in visited or not can(cur, nx):
                continue
            c_ = cp[nx[0]][nx[1]]
            if c_ > 0 and c_ != nxt_cp:
                continue
            if c_ == K and total - len(path) > 1:
                continue
            deg = sum(1 for a2, b2 in D if inb((nx[0] + a2, nx[1] + b2)) and (nx[0] + a2, nx[1] + b2) not in visited and can(nx, (nx[0] + a2, nx[1] + b2)))
            cands.append((nx, deg, c_))
        cands.sort(key=lambda t: (t[2] == 0, t[1]))
        ZV["moves"] = [list(p) for p, _, _ in cands]
        log(f"{ind}[node {node}] head {cur}, next checkpoint {nxt_cp}; moves (cell,free-degree) {[(p, d_) for p, d_, _ in cands]}")
        for nx, d_, c_ in cands:
            visited.add(nx)
            path.append(nx)
            dfs(nxt_cp + 1 if c_ == nxt_cp else nxt_cp, depth + 1)
            path.pop()
            visited.discard(nx)
            if len(st["sols"]) >= max_sol:
                return

    dfs(2, 0)
    return st["nodes"], st["sols"]


# --------------------------------------------------------------------------- Patches
def patches(path, check_unique=True, log=_pr):
    L = lines_of(path)
    R, C = map(int, L[0].split())
    i = L.index("GRID")
    clues = []  # (r,c,size|None,shape)
    for r in range(R):
        for c, t in enumerate(L[i + 1 + r].split()):
            if t == ".":
                continue
            m = re.fullmatch(r"(\d*)([STWF?]?)", t)
            clues.append((r, c, int(m.group(1)) if m.group(1) else None, m.group(2) if m.group(2) not in ("", "?") else None, t))
    total = R * C
    has = {(a[0], a[1]) for a in clues}
    minsz = [a[2] if a[2] else (2 if a[3] in ("T", "W") else 1) for a in clues]
    tmin = sum(minsz)
    if all(a[2] for a in clues) and sum(a[2] for a in clues) != total or tmin > total:
        return 0, []
    cands = []
    for k, (cr, cc, size, shape, tok) in enumerate(clues):
        lo, hi = minsz[k], total - (tmin - minsz[k])
        if size:
            lo = hi = size
        lst = []
        for h in range(1, R + 1):
            for w in range(1, C + 1):
                sz = h * w
                if sz < lo or sz > hi or (size and sz != size):
                    continue
                if shape == "S" and w != h or shape == "T" and h <= w or shape == "W" and w <= h:
                    continue
                for r0 in range(max(0, cr - h + 1), min(R - h, cr) + 1):
                    for c0 in range(max(0, cc - w + 1), min(C - w, cc) + 1):
                        cells = [(r, c) for r in range(r0, r0 + h) for c in range(c0, c0 + w)]
                        if any((r, c) in has and (r, c) != (cr, cc) for r, c in cells):
                            continue
                        lst.append((r0, c0, h, w, frozenset(cells)))
        log(f"clue {tok!r} at ({cr},{cc}): {len(lst)} candidate rectangle(s)")
        if not lst:
            return 0, []
        cands.append(lst)
    st = dict(nodes=0, sols=[])
    used = [False] * len(clues)
    chosen = []
    covered = set()
    PV = {"try": None}
    _raw = log
    def log(msg):
        _raw(msg.strip(), {"rects": [list(c) for c in chosen], "try": PV["try"]})
    max_sol = 2 if check_unique else 1

    def rec(depth):
        ind = "  " * depth
        st["nodes"] += 1
        node = st["nodes"]
        if len(chosen) == len(clues):
            if len(covered) == total:
                st["sols"].append(list(chosen))
                log(f"{ind}[node {node}] all clues placed, {total} cells covered -> SOLUTION")
            return
        best = None
        union = set()
        for k in range(len(clues)):
            if used[k]:
                continue
            compat = [x for x in cands[k] if not (x[4] & covered)]
            if not compat:
                log(f"{ind}[node {node}] clue {clues[k][4]!r} has 0 compatible rectangles -> prune")
                return
            for x in compat:
                union |= x[4]
            if best is None or len(compat) < len(best[1]):
                best = (k, compat)
        # cell-coverage prune (mirrors cell_prune in patches_solver.cpp; the C++ only enables it for <= 128 cells)
        if total <= 128:
            dead = [(r, c) for r in range(R) for c in range(C) if (r, c) not in covered and (r, c) not in union]
            if dead:
                log(f"{ind}[node {node}] cell {dead[0]} cannot be covered by any remaining rectangle -> prune")
                return
        k, compat = best
        log(f"{ind}[node {node}] MRV picks clue {clues[k][4]!r} at ({clues[k][0]},{clues[k][1]}) with {len(compat)} compatible rectangle(s)")
        used[k] = True
        for x in compat:
            chosen.append((clues[k][4], x[0], x[1], x[2], x[3]))
            covered.update(x[4])
            PV["try"] = None
            log(f"{ind}  place {x[2]}x{x[3]} at ({x[0]},{x[1]})")
            rec(depth + 1)
            covered.difference_update(x[4])
            chosen.pop()
            if len(st["sols"]) >= max_sol:
                return
        used[k] = False

    rec(0)
    return st["nodes"], st["sols"]


# --------------------------------------------------------------------------- Wend
_WORDS_CACHE = {}


def load_words(path=None):
    path = path or LEX   # LEX may be reassigned (the viewer generator points it at the frozen 197 k-word list)
    if path not in _WORDS_CACHE:
        _WORDS_CACHE[path] = {w.strip().upper() for w in open(path) if w.strip() and not w.startswith("#")}
    return _WORDS_CACHE[path]


def wend(path, check_unique=True, log=_pr):
    L = lines_of(path)
    R, C = map(int, L[0].split())
    i = L.index("GRID")
    g = [L[i + 1 + r].split() for r in range(R)]
    lens = [int(x) for x in next(l for l in L if l.startswith("LENGTHS")).split(":")[1].split()]
    words = load_words()
    prefixes = {w[:k] for w in words for k in range(1, len(w) + 1)}
    maxl = max(lens)
    D = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    void = lambda p: g[p[0]][p[1]] == "."
    cand = []

    def walk(p, word, pth):
        if len(pth) in lens and word in words:
            cand.append((word, tuple(pth)))
        if len(pth) >= maxl:
            return
        for a, b in D:
            q = (p[0] + a, p[1] + b)
            if 0 <= q[0] < R and 0 <= q[1] < C and not void(q) and q not in pth:
                w2 = word + g[q[0]][q[1]]
                if w2 in prefixes:
                    walk(q, w2, pth + [q])

    for r in range(R):
        for c in range(C):
            if not void((r, c)) and g[r][c] in prefixes:
                walk((r, c), g[r][c], [(r, c)])
    log(f"stage 1: {len(cand)} word paths enumerated: " + ", ".join(w for w, _ in cand))
    cells = R * C
    by_cell = [[] for _ in range(cells)]
    for k, (w, pth) in enumerate(cand):
        for (r, c) in pth:
            by_cell[r * C + c].append(k)
    rem = {}
    for x in lens:
        rem[x] = rem.get(x, 0) + 1
    cov = [void((r, c)) for r in range(R) for c in range(C)]
    st = dict(nodes=0, sols=[])
    ch = []
    WV = {"hl": None}
    _raw = log
    def log(msg):
        _raw(msg.strip(), {"placed": [{"word": cand[k][0], "cells": [list(c) for c in cand[k][1]]} for k in ch], "hl": WV["hl"]})
    max_sol = 2 if check_unique else 1

    def rec(depth):
        ind = "  " * depth
        st["nodes"] += 1
        node = st["nodes"]
        if all(cov):
            if all(v == 0 for v in rem.values()):
                st["sols"].append([cand[k] for k in ch])
                log(f"{ind}[node {node}] every cell covered, all lengths used -> SOLUTION")
            return
        best = None
        for ce in range(cells):
            if cov[ce]:
                continue
            ok = [k for k in by_cell[ce] if rem.get(len(cand[k][1]), 0) > 0 and not any(cov[r * C + c] for r, c in cand[k][1])]
            if not ok:
                log(f"{ind}[node {node}] uncovered cell ({ce // C},{ce % C}) has 0 usable paths -> dead end")
                return
            if best is None or len(ok) < len(best[1]):
                best = (ce, ok)
                if len(ok) == 1:
                    break
        ce, ok = best
        WV["hl"] = [ce // C, ce % C]
        log(f"{ind}[node {node}] MRV picks cell ({ce // C},{ce % C}) with {len(ok)} usable path(s): {[cand[k][0] for k in ok]}")
        for k in ok:
            if any(cov[r * C + c] for r, c in cand[k][1]) or rem.get(len(cand[k][1]), 0) <= 0:
                continue
            for r, c in cand[k][1]:
                cov[r * C + c] = True
            rem[len(cand[k][1])] -= 1
            ch.append(k)
            log(f"{ind}  place {cand[k][0]}")
            rec(depth + 1)
            ch.pop()
            rem[len(cand[k][1])] += 1
            for r, c in cand[k][1]:
                cov[r * C + c] = False
            if len(st["sols"]) >= max_sol:
                return

    rec(0)
    return st["nodes"], st["sols"]


# --------------------------------------------------------------------------- Crossclimb
def crossclimb(path, check_unique=True, log=_pr):
    L = lines_of(path)
    W = int(next(l for l in L if l.startswith("WORD_LENGTH")).split(":")[1])
    val = lambda key: next((l.split(":", 1)[1].split() for l in L if l.startswith(key)), None)
    start, end = val("START:"), val("END:")
    sc, ec = val("START_CANDIDATES"), val("END_CANDIDATES")
    j = L.index("RUNGS:")
    rungs = []
    for l in L[j + 1:]:
        if ":" in l:
            break
        rungs.append(l.strip())
    words = load_words()
    rs = set(rungs)
    ham = lambda a, b: sum(x != y for x, y in zip(a, b))

    def nbrs(w):
        out = set()
        for p in range(W):
            for ch in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
                if ch != w[p]:
                    x = w[:p] + ch + w[p + 1:]
                    if x in words and x not in rs:
                        out.add(x)
        return out

    def pool(single, cands):
        if single and single != ["?"]:
            return [single[0]] if single[0] in words and single[0] not in rs else []
        if cands:
            return [x for x in cands if x in words and x not in rs]
        return sorted(set().union(*[nbrs(r) for r in rungs]))

    sp, ep = pool(start, sc), pool(end, ec)
    K = len(rungs)
    adj = [[j2 for j2 in range(K) if j2 != i2 and ham(rungs[i2], rungs[j2]) == 1] for i2 in range(K)]
    log(f"start pool {sp}; end pool {ep}; rung graph (d_H=1) "
        + "; ".join(f"{rungs[i2]}:{[rungs[j2] for j2 in adj[i2]]}" for i2 in range(K)))
    if not sp or not ep:
        return 0, []
    st = dict(nodes=0, sols=[])
    max_sol = 2 if check_unique else 1
    pth = []
    XV = {"s": None}
    _raw = log
    def log(msg):
        _raw(msg.strip(), {"start": XV["s"], "ladder": [rungs[x] for x in pth], "rungs": rungs})

    def dfs(si, cur, mask, depth):
        XV["s"] = sp[si]
        ind = "  " * depth
        st["nodes"] += 1
        node = st["nodes"]
        if len(pth) == K:
            for e in ep:
                if ham(e, rungs[cur]) == 1 and e != sp[si]:
                    st["sols"].append([sp[si]] + [rungs[x] for x in pth] + [e])
                    log(f"{ind}[node {node}] all {K} rungs used; last rung {rungs[cur]} touches end {e} -> LADDER {' -> '.join(st['sols'][-1])}")
                    if len(st["sols"]) >= max_sol:
                        return
            return
        log(f"{ind}[node {node}] at {rungs[cur]}; unused neighbours {[rungs[x] for x in adj[cur] if not mask >> x & 1]}")
        for nx in adj[cur]:
            if not mask >> nx & 1:
                pth.append(nx)
                dfs(si, nx, mask | 1 << nx, depth + 1)
                pth.pop()
                if len(st["sols"]) >= max_sol:
                    return

    for si, s in enumerate(sp):
        for r0 in range(K):
            if ham(s, rungs[r0]) == 1:
                pth.append(r0)
                dfs(si, r0, 1 << r0, 0)
                pth.pop()
                if len(st["sols"]) >= max_sol:
                    return st["nodes"], st["sols"]
    return st["nodes"], st["sols"]


GAMES = dict(queens=queens, sudoku=sudoku, tango=tango, zip=zip_, patches=patches, wend=wend, crossclimb=crossclimb)

CHECK = [
    ("queens", "fixtures/valid_4x4_multiple.txt"), ("queens", "fixtures/valid_8x8_unique.txt"),
    ("sudoku", "fixtures/sudoku_branching.txt"), ("sudoku", "fixtures/sudoku_valid_multiple.txt"),
    ("tango", "fixtures/corpus/tango/easy/tango_easy_1.txt"), ("tango", "fixtures/tango_edge_clue_dependent.txt"),
    ("tango", "fixtures/tango_edge_clue_ignored.txt"), ("tango", "fixtures/corpus/tango/hard/tango_hard_1.txt"),
    ("zip", "fixtures/corpus/zip/easy/zip_easy_1.txt"), ("zip", "fixtures/zip_checkpoint_order.txt"),
    ("zip", "fixtures/corpus/zip/hard/zip_hard_1.txt"),
    ("patches", "fixtures/patches_freeform.txt"), ("patches", "fixtures/patches_valid_unique.txt"),
    ("patches", "fixtures/corpus/patches/hard/patches_hard_1.txt"),
    ("wend", "fixtures/corpus/wend/easy/wend_easy_1.txt"), ("wend", "fixtures/wend_repeated_letters.txt"),
    ("wend", "fixtures/wend_ambiguous.txt"),
    ("crossclimb", "fixtures/crossclimb_valid_unique.txt"), ("crossclimb", "fixtures/crossclimb_ambiguous.txt"),
    ("crossclimb", "fixtures/crossclimb_missing_endpoints.txt"),
]


def cli_nodes(game, f):
    args = [CLI, game, "solve", os.path.join(ROOT, f), "--check-unique", "--stats"]
    if game == "queens":
        args += ["--solver", "bitset"]
    o = subprocess.run(args, capture_output=True, text=True, stdin=subprocess.DEVNULL).stdout
    m = re.search(r"Nodes visited\s*:\s*(\d+)", o)
    return int(m.group(1)) if m else None, o


def main():
    if len(sys.argv) >= 4 and sys.argv[1] == "trace":
        GAMES[sys.argv[2]](os.path.join(ROOT, sys.argv[3]) if not os.path.isabs(sys.argv[3]) else sys.argv[3])
        return
    bad = 0
    for game, f in CHECK:
        mine, sols = GAMES[game](os.path.join(ROOT, f), True, log=lambda *_: None)
        theirs, out = cli_nodes(game, f)
        ok = mine == theirs
        bad += not ok
        print(f"{'OK  ' if ok else 'DIFF'} {game:10s} {f:55s} python-mirror nodes={mine}  C++ nodes={theirs}  solutions_found={len(sols)}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
