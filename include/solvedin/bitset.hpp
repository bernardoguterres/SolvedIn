#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace solvedin {

/// A fast multi-word bitset supporting arbitrary bit lengths,
/// bit iteration via hardware CTZ, and word-boundary masking.
class BitSet {
public:
    BitSet() : num_bits_(0) {}

    explicit BitSet(size_t num_bits, bool init_val = false)
        : num_bits_(num_bits), words_((num_bits + 63) / 64, init_val ? ~0ULL : 0ULL) {
        if (init_val) {
            sanitize_last_word();
        }
    }

    [[nodiscard]] size_t size() const noexcept { return num_bits_; }
    [[nodiscard]] size_t num_words() const noexcept { return words_.size(); }
    [[nodiscard]] const std::vector<uint64_t>& words() const noexcept { return words_; }

    void set(size_t bit) {
        assert(bit < num_bits_);
        words_[bit / 64] |= (1ULL << (bit % 64));
    }

    void reset(size_t bit) {
        assert(bit < num_bits_);
        words_[bit / 64] &= ~(1ULL << (bit % 64));
    }

    [[nodiscard]] bool test(size_t bit) const {
        assert(bit < num_bits_);
        return (words_[bit / 64] & (1ULL << (bit % 64))) != 0;
    }

    void set_all() {
        for (auto& w : words_) {
            w = ~0ULL;
        }
        sanitize_last_word();
    }

    void reset_all() {
        for (auto& w : words_) {
            w = 0ULL;
        }
    }

    [[nodiscard]] size_t count() const noexcept {
        size_t total = 0;
        for (uint64_t w : words_) {
            total += static_cast<size_t>(std::popcount(w));
        }
        return total;
    }

    [[nodiscard]] bool any() const noexcept {
        for (uint64_t w : words_) {
            if (w != 0) return true;
        }
        return false;
    }

    [[nodiscard]] bool none() const noexcept {
        return !any();
    }

    [[nodiscard]] int find_first() const noexcept {
        for (size_t w = 0; w < words_.size(); ++w) {
            if (words_[w] != 0) {
                int ctz = std::countr_zero(words_[w]);
                size_t bit = w * 64 + static_cast<size_t>(ctz);
                return (bit < num_bits_) ? static_cast<int>(bit) : -1;
            }
        }
        return -1;
    }

    [[nodiscard]] int find_next(size_t prev_bit) const noexcept {
        size_t start = prev_bit + 1;
        if (start >= num_bits_) return -1;

        size_t w = start / 64;
        size_t bit_in_word = start % 64;

        uint64_t first_word = words_[w] & (~0ULL << bit_in_word);
        if (first_word != 0) {
            size_t bit = w * 64 + static_cast<size_t>(std::countr_zero(first_word));
            return (bit < num_bits_) ? static_cast<int>(bit) : -1;
        }

        for (size_t next_w = w + 1; next_w < words_.size(); ++next_w) {
            if (words_[next_w] != 0) {
                size_t bit = next_w * 64 + static_cast<size_t>(std::countr_zero(words_[next_w]));
                return (bit < num_bits_) ? static_cast<int>(bit) : -1;
            }
        }
        return -1;
    }

    template <typename Func>
    void for_each_set_bit(Func&& func) const {
        for (size_t w = 0; w < words_.size(); ++w) {
            uint64_t word = words_[w];
            while (word != 0) {
                int ctz = std::countr_zero(word);
                size_t bit = w * 64 + static_cast<size_t>(ctz);
                func(static_cast<int>(bit));
                word &= (word - 1);
            }
        }
    }

    BitSet& operator&=(const BitSet& other) noexcept {
        assert(num_bits_ == other.num_bits_);
        for (size_t i = 0; i < words_.size(); ++i) {
            words_[i] &= other.words_[i];
        }
        return *this;
    }

    BitSet& operator|=(const BitSet& other) noexcept {
        assert(num_bits_ == other.num_bits_);
        for (size_t i = 0; i < words_.size(); ++i) {
            words_[i] |= other.words_[i];
        }
        return *this;
    }

    BitSet& operator^=(const BitSet& other) noexcept {
        assert(num_bits_ == other.num_bits_);
        for (size_t i = 0; i < words_.size(); ++i) {
            words_[i] ^= other.words_[i];
        }
        sanitize_last_word();
        return *this;
    }

    [[nodiscard]] BitSet operator~() const {
        BitSet result = *this;
        for (size_t i = 0; i < result.words_.size(); ++i) {
            result.words_[i] = ~result.words_[i];
        }
        result.sanitize_last_word();
        return result;
    }

    bool operator==(const BitSet& other) const noexcept {
        return num_bits_ == other.num_bits_ && words_ == other.words_;
    }

private:
    size_t num_bits_{0};
    std::vector<uint64_t> words_;

    void sanitize_last_word() noexcept {
        if (num_bits_ == 0) return;
        size_t rem = num_bits_ % 64;
        if (rem != 0) {
            uint64_t mask = (1ULL << rem) - 1ULL;
            words_.back() &= mask;
        }
    }
};

inline BitSet operator&(BitSet lhs, const BitSet& rhs) noexcept {
    lhs &= rhs;
    return lhs;
}

inline BitSet operator|(BitSet lhs, const BitSet& rhs) noexcept {
    lhs |= rhs;
    return lhs;
}

inline BitSet operator^(BitSet lhs, const BitSet& rhs) noexcept {
    lhs ^= rhs;
    return lhs;
}

} // namespace solvedin
