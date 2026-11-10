#pragma once

#include <array>
#include <cctype>
#include <istream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace solvedin {

struct TrieNode {
    std::array<std::unique_ptr<TrieNode>, 26> children{};
    bool is_word{false};
};

class Trie {
public:
    Trie();
    ~Trie() = default;

    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;
    Trie(Trie&&) noexcept = default;
    Trie& operator=(Trie&&) noexcept = default;

    void insert(std::string_view word);
    [[nodiscard]] bool contains(std::string_view word) const;
    [[nodiscard]] bool has_prefix(std::string_view prefix) const;

    [[nodiscard]] const TrieNode* root() const noexcept { return root_.get(); }
    [[nodiscard]] static const TrieNode* get_child(const TrieNode* node, char c) noexcept {
        if (!node) return nullptr;
        char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (up < 'A' || up > 'Z') return nullptr;
        return node->children[up - 'A'].get();
    }

    [[nodiscard]] size_t size() const noexcept { return word_count_; }
    [[nodiscard]] bool empty() const noexcept { return word_count_ == 0; }

    static Trie load_from_file(const std::string& filepath);
    static Trie load_from_stream(std::istream& is);
    static Trie get_default_lexicon();

private:
    std::unique_ptr<TrieNode> root_;
    size_t word_count_{0};
};

} // namespace solvedin
