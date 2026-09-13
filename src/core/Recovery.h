#pragma once
#include "core/FileIO.h"
namespace kiri {
struct Draft {
    std::string path,name,text,error;
    FileStamp base;
    uint64_t caret=0,anchor=0,firstLine=0;
    bool bom=false;
    int eol=2;
    bool ok() const { return error.empty(); }
};
// Atomic, checksummed private files. Never writes to the document's source path.
std::string WriteDraft(const std::string& file,const Draft& draft);
Draft ReadDraft(const std::string& file,const std::function<bool(std::string_view)>& sink={});
std::vector<std::string> ListDrafts(const std::string& directory);
}
