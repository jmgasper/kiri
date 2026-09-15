#pragma once
#include "core/FileIO.h"
#include <atomic>
#include <string>
#include <vector>

namespace kiri {
constexpr size_t kDiffBytes=8*1024*1024,kDiffLines=100000;
struct DiffSource {
    std::string path,label,text,error;
    bool exists=true,binary=false;
    uint64_t size=0;
    FileStamp stamp;
};
struct DiffLine { size_t start=0,length=0,ending=0; };
struct DiffRow { int before=-1,after=-1,hunk=-1; };
// Zero-based source line ranges, independent of presentation and context lines.
struct DiffHunk { size_t row=0,rows=0,before=0,removed=0,after=0,added=0; };
struct DiffModel {
    DiffSource before,after;
    std::vector<DiffLine> leftLines,rightLines;
    std::vector<DiffRow> rows;
    std::vector<DiffHunk> hunks;
    std::string error;
    bool binary=false;
    std::string Unified(std::vector<size_t>* hunkLines=nullptr) const;
};
DiffSource DiskDiffSource(const std::string& path,const std::string& label,const std::atomic<bool>* cancel=nullptr);
DiffModel CompareText(DiffSource before,DiffSource after,const std::atomic<bool>* cancel=nullptr,size_t workLimit=8000000);
std::string DiffDescription(const DiffSource& source);
// Byte ranges for the unequal middle, always on UTF-8 character boundaries.
struct ChangedText { size_t prefix=0,beforeEnd=0,afterEnd=0; };
ChangedText ChangedMiddle(std::string_view before,std::string_view after);
}
