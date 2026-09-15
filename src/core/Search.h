#pragma once
#include "core/LanguageProtocol.h"
#include <atomic>
#include <memory>

namespace kiri {
struct SearchOptions {
    std::string query;
    bool matchCase=false,regex=false,wholeWord=false;
};
struct TextMatches {
    std::vector<TextEdit> matches;
    std::string error;
    bool truncated=false,cancelled=false;
};
// PCRE2 UTF-8, Unicode properties, CR/LF/CRLF anchors. Positions are byte offsets.
// Regex replacements: $0..$99, ${name}, $$, \\n, \\r, \\t, \\\\.
class TextQuery {
public:
    explicit TextQuery(const SearchOptions& options);
    ~TextQuery();
    TextQuery(const TextQuery&)=delete;
    TextQuery& operator=(const TextQuery&)=delete;
    const std::string& Error() const;
    std::string ValidateReplacement(const std::string& replacement) const;
    TextMatches Find(std::string_view text,size_t start=0,size_t end=SIZE_MAX,
        size_t limit=100000,const std::atomic<bool>* cancel=nullptr,
        const std::string* replacement=nullptr) const;
private:
    struct Impl;
    std::unique_ptr<Impl> fImpl;
};
}
