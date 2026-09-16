#pragma once
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>
#include "md4c/md4c.h"

namespace kiri {
constexpr size_t kMarkdownBytes=2*1024*1024, kMarkdownLines=30000;
enum MarkdownStyle { MarkdownBold=1, MarkdownItalic=2, MarkdownCode=4, MarkdownStrike=8 };
struct MarkdownRun {
    std::string text,link,image;
    unsigned style=0;
    size_t line=0;
};
struct MarkdownBlock {
    MD_BLOCKTYPE type=MD_BLOCK_DOC;
    std::vector<size_t> children;
    std::vector<MarkdownRun> runs;
    size_t firstLine=0,lastLine=0;
    unsigned level=0,start=1;
    MD_ALIGN align=MD_ALIGN_DEFAULT;
    char task=0;
    std::string anchor;
};
struct MarkdownDocument {
    std::vector<MarkdownBlock> blocks;
    size_t lines=1;
    std::string error;
};
MarkdownDocument ParseMarkdown(const std::string& text,const std::atomic<bool>* cancel=nullptr);
enum class MarkdownTargetKind { Blocked, Anchor, Local, Web };
struct MarkdownTarget {
    MarkdownTargetKind kind=MarkdownTargetKind::Blocked;
    std::string path,fragment,reason;
};
// No network access occurs here. Only explicit link activation may launch HTTP(S).
MarkdownTarget ResolveMarkdownTarget(const std::string& target,const std::string& documentPath,bool image=false);
std::string MarkdownAnchor(const std::string& text);
}
