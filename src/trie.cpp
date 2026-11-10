#include "solvedin/trie.hpp"

#include <fstream>
#include <sstream>

namespace solvedin {

namespace {

std::string normalize_word(std::string_view word) {
    std::string norm;
    norm.reserve(word.size());
    for (char c : word) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            norm.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    return norm;
}

// Embedded default lexicon with common words of lengths 3, 4, 5, 6
const char* const DEFAULT_WORDS[] = {
    // 3-letter words
    "ACT", "ACE", "AGE", "AIR", "AND", "ANT", "APE", "ARM", "ART", "BAD",
    "BAG", "BAN", "BAR", "BAT", "BAY", "BED", "BEE", "BET", "BID", "BIG",
    "BIN", "BIT", "BOB", "BOG", "BOW", "BOX", "BOY", "BUS", "BUT", "BUY",
    "CAN", "CAP", "CAR", "CAT", "COG", "COT", "COW", "CRY", "CUP", "CUT",
    "DAD", "DAM", "DAY", "DEN", "DEW", "DID", "DIG", "DIM", "DIP", "DOG",
    "DOT", "DRY", "DUE", "EAR", "EAT", "EGG", "END", "EYE", "FAN", "FAR",
    "FAT", "FEE", "FEW", "FIG", "FIT", "FLY", "FOG", "FOR", "FOX", "FUN",
    "FUR", "GAP", "GAS", "GEL", "GET", "GOD", "GUN", "GUY", "HAT", "HAY",
    "HEN", "HER", "HEY", "HIM", "HIP", "HIT", "HOG", "HOT", "HOW", "HUB",
    "HUG", "ICE", "ILL", "INK", "INN", "ION", "JAR", "JAW", "JAY", "JET",
    "JOB", "JOG", "JOY", "KEY", "KID", "KIT", "LAB", "LAP", "LAW", "LAY",
    "LEG", "LET", "LID", "LIE", "LIP", "LOG", "LOT", "LOW", "MAD", "MAN",
    "MAP", "MAT", "MAY", "MEN", "MUD", "MUG", "NET", "NEW", "NIL", "NOD",
    "NOT", "NOW", "NUT", "OAK", "OAR", "OAT", "ODD", "OFF", "OIL", "OLD",
    "ONE", "OUR", "OUT", "OWL", "PAN", "PAY", "PEA", "PEG", "PEN", "PET",
    "PIE", "PIG", "PIN", "PIT", "POT", "PUB", "RAG", "RAM", "RAN", "RAT",
    "RAW", "RAY", "RED", "RIB", "RID", "RIG", "RIM", "RIP", "ROB", "ROD",
    "ROW", "RUB", "RUG", "RUN", "RUT", "SAD", "SAG", "SAP", "SAT", "SAW",
    "SAY", "SEA", "SEE", "SET", "SEW", "SHY", "SIN", "SIP", "SIR", "SIT",
    "SKI", "SKY", "SON", "SOW", "SPY", "SUN", "TAB", "TAG", "TAN", "TAP",
    "TAR", "TAX", "TEA", "TEN", "THE", "TIE", "TIN", "TIP", "TOE", "TOP",
    "TOY", "TUB", "TWO", "URN", "USE", "VAN", "VAT", "VET", "VIA", "VOW",
    "WAR", "WAY", "WEB", "WET", "WHO", "WIG", "WIN", "WIT", "YAK", "YAM",
    "YAP", "YES", "YET", "ZIP", "ZOO",

    // 4-letter words
    "ACID", "AGED", "ALLY", "ALSO", "ARCH", "AREA", "ARMY", "AWAY", "BABY", "BACK",
    "BAKE", "BALL", "BAND", "BANK", "BARE", "BARK", "BARN", "BASE", "BASS", "BEAD",
    "BEAM", "BEAN", "BEAR", "BEAT", "BEEF", "BEER", "BELL", "BELT", "BEND", "BEST",
    "BILL", "BIND", "BIRD", "BITE", "BLOW", "BLUE", "BOAT", "BODY", "BOIL", "BOLD",
    "BOMB", "BOND", "BONE", "BOOK", "BOOM", "BOOT", "BORN", "BOSS", "BOTH", "BOWL",
    "BULK", "BURN", "BUSH", "BUSY", "CALL", "CALM", "CAME", "CAMP", "CARD", "CARE",
    "CART", "CASE", "CASH", "CAST", "CATS", "CAVE", "CELL", "CHIP", "CITY", "CLAW",
    "CLAY", "CLIP", "CLUB", "COAL", "COAT", "CODE", "COLD", "COME", "COOK", "COOL",
    "COPE", "COPY", "CORD", "CORE", "CORN", "COST", "CREW", "CROP", "CROW", "CURE",
    "DARK", "DATA", "DATE", "DAWN", "DAYS", "DEAD", "DEAF", "DEAL", "DEAN", "DEAR",
    "DEBT", "DEEP", "DEER", "DESK", "DIAL", "DIRT", "DISC", "DISK", "DOGS", "DOOR",
    "DOSE", "DOWN", "DRAW", "DROP", "DRUG", "DRUM", "DUCK", "DUST", "DUTY", "EACH",
    "EARN", "EAST", "EASY", "EDGE", "ELSE", "EVEN", "EVER", "FACE", "FACT", "FAIL",
    "FAIR", "FALL", "FARM", "FAST", "FATE", "FEAR", "FEED", "FEEL", "FEET", "FELL",
    "FILE", "FILL", "FILM", "FIND", "FINE", "FIRE", "FIRM", "FISH", "FIVE", "FLAT",
    "FLOW", "FOOD", "FOOT", "FORD", "FORM", "FORT", "FOUR", "FREE", "FROM", "FUEL",
    "FULL", "FUND", "GAIN", "GAME", "GATE", "GEAR", "GIFT", "GIRL", "GIVE", "GLAD",
    "GOAL", "GOES", "GOLD", "GOLF", "GONE", "GOOD", "GRAY", "GREW", "GROW", "GULF",
    "HAIR", "HALF", "HALL", "HAND", "HANG", "HARD", "HARM", "HATE", "HAVE", "HEAD",
    "HEAR", "HEAT", "HELD", "HELL", "HELP", "HERE", "HERO", "HIGH", "HILL", "HIRE",
    "HOLD", "HOLE", "HOLY", "HOME", "HOPE", "HORN", "HOSE", "HOST", "HOUR", "HUGE",
    "HUNG", "HUNT", "HURT", "IDEA", "INCH", "INTO", "IRON", "ITEM", "JACK", "JANE",
    "JEAN", "JOIN", "JUMP", "JURY", "JUST", "KEEN", "KEEP", "KENT", "KEPT", "KICK",
    "KILL", "KIND", "KING", "KNEE", "KNEW", "KNOW", "LACK", "LADY", "LAID", "LAKE",
    "LAND", "LANE", "LAST", "LATE", "LEAD", "LEFT", "LESS", "LIFE", "LIFT", "LIKE",
    "LINE", "LINK", "LION", "LIPS", "LIST", "LIVE", "LOAD", "LOAN", "LOCK", "LOGO",
    "LONG", "LOOK", "LORD", "LOSE", "LOSS", "LOST", "LOVE", "LUCK", "MADE", "MAIL",
    "MAIN", "MAKE", "MALE", "MANY", "MARK", "MASS", "MEAL", "MEAN", "MEAT", "MEET",
    "MENU", "MERE", "MIKE", "MILE", "MILK", "MILL", "MIND", "MINE", "MISS", "MODE",
    "MOOD", "MOON", "MORE", "MOST", "MOVE", "MUCH", "MUST", "NAME", "NAVY", "NEAR",
    "NECK", "NEED", "NEWS", "NEXT", "NICE", "NICK", "NINE", "NONE", "NOSE", "NOTE",
    "OKAY", "ONCE", "ONLY", "OPEN", "ORAL", "OVER", "PACE", "PACK", "PAGE", "PAID",
    "PAIN", "PAIR", "PALM", "PARK", "PART", "PASS", "PAST", "PATH", "PEAK", "PEER",
    "PICK", "PILE", "PINE", "PINK", "PIPE", "PLAN", "PLAY", "PLOT", "PLUG", "PLUS",
    "POEM", "POET", "POLE", "POND", "POOL", "POOR", "PORT", "POST", "POUR", "PULL",
    "PURE", "PUSH", "RACE", "RAIL", "RAIN", "RANK", "RARE", "RATE", "READ", "REAL",
    "REAR", "RELY", "RENT", "REST", "RICE", "RICH", "RIDE", "RING", "RISE", "RISK",
    "ROAD", "ROCK", "ROLE", "ROLL", "ROOF", "ROOM", "ROOT", "ROSE", "RULE", "RUSH",
    "SAFE", "SAID", "SAIL", "SALE", "SAME", "SAND", "SAVE", "SEAT", "SEED", "SEEK",
    "SEEM", "SEEN", "SELF", "SELL", "SEND", "SENT", "SEPT", "SHIP", "SHOP", "SHOT",
    "SHOW", "SHUT", "SICK", "SIDE", "SIGN", "SILK", "SITE", "SIZE", "SKIN", "SLOW",
    "SNOW", "SOAP", "SOFT", "SOIL", "SOLD", "SOLE", "SOME", "SONG", "SOON", "SORT",
    "SOUL", "SOUP", "SPAN", "SPIN", "SPOT", "STAR", "STAY", "STEP", "STOP", "SUCH",
    "SUIT", "SURE", "SWIM", "TAKE", "TALE", "TALK", "TALL", "TANK", "TAPE", "TASK",
    "TEAM", "TELL", "TENT", "TERM", "TEST", "TEXT", "THAT", "THEM", "THEN", "THEY",
    "THIN", "THIS", "THOU", "TIDE", "TIED", "TIEP", "TIER", "TILL", "TIME", "TINY",
    "TIRE", "TOLL", "TONE", "TOOK", "TOOL", "TOUR", "TOWN", "TREE", "TRIP", "TRUE",
    "TUBE", "TUNE", "TURN", "TWIN", "TYPE", "UNIT", "UPON", "USED", "USER", "VARY",
    "VAST", "VERY", "VICE", "VIEW", "VOTE", "WAGE", "WAIT", "WAKE", "WALK", "WALL",
    "WANT", "WARD", "WARM", "WARN", "WASH", "WAVE", "WEAK", "WEAR", "WEEK", "WELL",
    "WENT", "WERE", "WEST", "WHAT", "WHEN", "WHOM", "WIDE", "WIFE", "WILD", "WILL",
    "WIND", "WINE", "WING", "WIRE", "WISE", "WISH", "WITH", "WOOD", "WOOL", "WORD",
    "WORE", "WORK", "YARD", "YEAH", "YEAR", "ZERO", "ZONE",

    // 5-letter words
    "ABOUT", "ABOVE", "ACTOR", "ACUTE", "ADULT", "AFTER", "AGAIN", "AGENT", "AGREE", "AHEAD",
    "ALARM", "ALBUM", "ALERT", "ALIKE", "ALIVE", "ALLOW", "ALONE", "ALONG", "ALTER", "AMONG",
    "ANGER", "ANGLE", "ANGRY", "APART", "APPLE", "APPLY", "ARENA", "ARGUE", "ARISE", "ARMED",
    "ASSET", "AVOID", "AWARD", "AWARE", "BADLY", "BAKER", "BASES", "BASIC", "BASIS", "BEACH",
    "BEGAN", "BEGIN", "BEING", "BELOW", "BENCH", "BIRTH", "BLACK", "BLAME", "BLIND", "BLOCK",
    "BLOOD", "BOARD", "BOAST", "BOOST", "BOOTH", "BOUND", "BRAIN", "BRAND", "BREAD", "BREAK",
    "BREED", "BRIEF", "BRING", "BROAD", "BROKE", "BROWN", "BUILD", "BUILT", "BUYER", "CABLE",
    "CALIF", "CARRY", "CATCH", "CAUSE", "CHAIN", "CHAIR", "CHART", "CHASE", "CHEAP", "CHECK",
    "CHEST", "CHIEF", "CHILD", "CHINA", "CHOSE", "CIVIL", "CLAIM", "CLASS", "CLEAN", "CLEAR",
    "CLICK", "CLOCK", "CLOSE", "COACH", "COAST", "COULD", "COUNT", "COURT", "COVER", "CRAFT",
    "CRANE", "CRASH", "CREAM", "CRIME", "CROSS", "CROWD", "CROWN", "CURVE", "CYCLE", "DAILY",
    "DANCE", "DATED", "DEALT", "DEATH", "DEBUT", "DELAY", "DEPTH", "DOING", "DOUBT", "DOZEN",
    "DRAFT", "DRAMA", "DRAWN", "DREAM", "DRESS", "DRILL", "DRINK", "DRIVE", "DROVE", "DYING",
    "EAGER", "EARLY", "EARTH", "EIGHT", "ELITE", "EMPTY", "ENEMY", "ENJOY", "ENTER", "ENTRY",
    "EQUAL", "ERROR", "EVENT", "EVERY", "EXACT", "EXIST", "EXTRA", "FAITH", "FALSE", "FAULT",
    "FIBER", "FIELD", "FIFTH", "FIFTY", "FIGHT", "FINAL", "FIRST", "FIXED", "FLASH", "FLEET",
    "FLOOR", "FLUID", "FOCUS", "FORCE", "FORTH", "FORTY", "FORUM", "FOUND", "FRAME", "FRANK",
    "FRAUD", "FRESH", "FRONT", "FRUIT", "FULLY", "FUNNY", "GIANT", "GIVEN", "GLASS", "GLOBE",
    "GOING", "GRACE", "GRADE", "GRAND", "GRANT", "GRASS", "GREAT", "GREEN", "GROSS", "GROUP",
    "GROWN", "GUARD", "GUESS", "GUEST", "GUIDE", "HAPPY", "HARRY", "HEART", "HEAVY", "HENCE",
    "HORSE", "HOTEL", "HOUSE", "HUMAN", "IDEAL", "IMAGE", "INDEX", "INNER", "INPUT", "ISSUE",
    "JAPAN", "JIMMY", "JOINT", "JONES", "JUDGE", "KNOWN", "LABEL", "LARGE", "LASER", "LATER",
    "LAUGH", "LAYER", "LEARN", "LEASE", "LEAST", "LEAVE", "LEGAL", "LEVEL", "LEWIS", "LIGHT",
    "LIMIT", "LINKS", "LIVES", "LOCAL", "LOGIC", "LOOSE", "LOWER", "LUCKY", "LUNCH", "LYING",
    "MAGIC", "MAJOR", "MAKER", "MARCH", "MATCH", "MAYBE", "MAYOR", "MEANT", "MEDIA", "METAL",
    "MIGHT", "MINOR", "MINUS", "MIXED", "MODEL", "MONEY", "MONTH", "MORAL", "MOTOR", "MOUNT",
    "MOUSE", "MOUTH", "MOVIE", "MUSIC", "NEEDS", "NEVER", "NEWLY", "NIGHT", "NOISE", "NORTH",
    "NOTED", "NOVEL", "NURSE", "OCCUR", "OCEAN", "OFFER", "OFTEN", "ORDER", "OTHER", "OUGHT",
    "PAINT", "PANEL", "PAPER", "PARTY", "PEACE", "PETER", "PHASE", "PHONE", "PHOTO", "PIECE",
    "PILOT", "PITCH", "PLACE", "PLAIN", "PLANE", "PLANT", "PLATE", "POINT", "POUND", "POWER",
    "PRESS", "PRICE", "PRIDE", "PRIME", "PRINT", "PRIOR", "PRIZE", "PROOF", "PROUD", "PROVE",
    "QUEEN", "QUICK", "QUIET", "QUITE", "RADIO", "RAISE", "RANGE", "RAPID", "RATIO", "REACH",
    "REACT", "READY", "REFER", "RIGHT", "RIVAL", "RIVER", "ROBIN", "ROGER", "ROMAN", "ROUGH",
    "ROUND", "ROUTE", "ROYAL", "RURAL", "SCALE", "SCENE", "SCOPE", "SCORE", "SENSE", "SERVE",
    "SEVEN", "SHALL", "SHAPE", "SHARE", "SHARP", "SHEET", "SHELF", "SHELL", "SHIFT", "SHIRT",
    "SHOCK", "SHOOT", "SHORT", "SHOWN", "SIGHT", "SINCE", "SIXTY", "SIZED", "SKILL", "SLEEP",
    "SLIDE", "SMALL", "SMART", "SMILE", "SMITH", "SMOKE", "SOLID", "SOLVE", "SORRY", "SOUND",
    "SOUTH", "SPACE", "SPARE", "SPEAK", "SPEED", "SPEND", "SPENT", "SPLIT", "SPOKE", "SPORT",
    "STAFF", "STAGE", "STAKE", "STAND", "START", "STATE", "STEAM", "STEEL", "STICK", "STILL",
    "STOCK", "STONE", "STOOD", "STORE", "STORM", "STORY", "STRIP", "STUDY", "STUFF", "STYLE",
    "SUGAR", "SUITE", "SUPER", "SWEET", "TABLE", "TAKEN", "TASTE", "TAXES", "TEACH", "TEETH",
    "TERRY", "TEXAS", "THANK", "THEFT", "THEIR", "THEME", "THERE", "THESE", "THICK", "THING",
    "THINK", "THIRD", "THOSE", "THREE", "THREW", "THROW", "TIGHT", "TIMES", "TIRED", "TITLE",
    "TODAY", "TOPIC", "TOTAL", "TOUCH", "TOUGH", "TOWER", "TRACK", "TRADE", "TRAIN", "TREAT",
    "TREND", "TRIAL", "TRIED", "TRIES", "TRUCK", "TRULY", "TRUST", "TRUTH", "TWICE", "UNDER",
    "UNDUE", "UNION", "UNITY", "UNTIL", "UPPER", "UPSET", "URBAN", "USAGE", "USUAL", "VALID",
    "VALUE", "VIDEO", "VIRUS", "VISIT", "VITAL", "VOICE", "WASTE", "WATCH", "WATER", "WHEEL",
    "WHERE", "WHICH", "WHILE", "WHITE", "WHOLE", "WHOSE", "WOMAN", "WOMEN", "WORLD", "WORRY",
    "WORSE", "WORST", "WORTH", "WOULD", "WOUND", "WRITE", "WRONG", "WROTE", "YIELD", "YOUNG",
    "YOUTH"
};

} // namespace

Trie::Trie() : root_(std::make_unique<TrieNode>()), word_count_(0) {}

void Trie::insert(std::string_view word) {
    std::string norm = normalize_word(word);
    if (norm.empty()) return;

    TrieNode* curr = root_.get();
    for (char c : norm) {
        int idx = c - 'A';
        if (!curr->children[idx]) {
            curr->children[idx] = std::make_unique<TrieNode>();
        }
        curr = curr->children[idx].get();
    }
    if (!curr->is_word) {
        curr->is_word = true;
        word_count_++;
    }
}

bool Trie::contains(std::string_view word) const {
    std::string norm = normalize_word(word);
    if (norm.empty()) return false;

    const TrieNode* curr = root_.get();
    for (char c : norm) {
        curr = get_child(curr, c);
        if (!curr) return false;
    }
    return curr->is_word;
}

bool Trie::has_prefix(std::string_view prefix) const {
    std::string norm = normalize_word(prefix);
    if (norm.empty()) return true;

    const TrieNode* curr = root_.get();
    for (char c : norm) {
        curr = get_child(curr, c);
        if (!curr) return false;
    }
    return true;
}

Trie Trie::load_from_stream(std::istream& is) {
    Trie trie;
    std::string line;
    while (std::getline(is, line)) {
        std::istringstream iss(line);
        std::string w;
        while (iss >> w) {
            trie.insert(w);
        }
    }
    return trie;
}

Trie Trie::load_from_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        return get_default_lexicon();
    }
    return load_from_stream(ifs);
}

Trie Trie::get_default_lexicon() {
    Trie trie;
    for (const char* w : DEFAULT_WORDS) {
        trie.insert(w);
    }
    return trie;
}

} // namespace solvedin
