#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <vector>

namespace kiri {
struct FileStamp {
    uint64_t device=0, inode=0, size=0;
    int64_t seconds=0, nanoseconds=0;
    bool exists=false;
    bool operator==(const FileStamp& other) const;
    bool operator!=(const FileStamp& other) const { return !(*this==other); }
};
struct FileData {
    std::string bytes;
    FileStamp stamp;
    std::string error;
    bool binary=false;
    bool utf8=true;
    bool bom=false;
    int eol=2; // Scintilla: 0 CRLF, 1 CR, 2 LF
    bool ok() const { return error.empty(); }
};
FileStamp StatFile(const std::string& path);
std::string CanonicalPath(const std::string& path);
bool IsValidUTF8(std::string_view bytes);
FileData ReadFile(const std::string& path, const std::atomic<bool>* cancel=nullptr,
    size_t limit=1024ULL*1024*1024,
    const std::function<bool(std::string_view)>& sink={});
// With a sink, bytes retains only a 64 KiB preview. UTF-8 BOM is omitted from
// delivered text; metadata still records it so saving can preserve it.
// Uses a same-directory temporary, flush, external-change recheck and rename.
// Existing permissions and Haiku attributes are preserved. An expected stamp
// is mandatory: a new file expects {exists=false}, an existing one its last read.
std::string SaveFile(const std::string& path, std::string_view bytes, const FileStamp& expected);
std::string SaveFileParts(const std::string& path,const std::vector<std::string_view>& parts,const FileStamp& expected);
std::string HexPreview(std::string_view bytes, uint64_t totalSize);
}
