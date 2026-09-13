#include "core/FileIO.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __HAIKU__
#include <fs_attr.h>
#include <dirent.h>
#endif

namespace kiri {
namespace {
class UTF8Validator {
public:
    void Feed(std::string_view bytes) {
        for(unsigned char c:bytes) {
            if(!valid) return;
            if(remaining) {
                if((c&0xc0)!=0x80) { valid=false;return; }
                code=(code<<6)|(c&63);
                if(--remaining==0 && (code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff))) valid=false;
            } else if(c<0x80) continue;
            else if(c>=0xc2 && c<=0xdf) { remaining=1;code=c&31;minimum=0x80; }
            else if(c>=0xe0 && c<=0xef) { remaining=2;code=c&15;minimum=0x800; }
            else if(c>=0xf0 && c<=0xf4) { remaining=3;code=c&7;minimum=0x10000; }
            else valid=false;
        }
    }
    bool Complete() const { return valid && remaining==0; }
private: bool valid=true;int remaining=0;uint32_t code=0,minimum=0;
};
FileStamp Stamp(const struct stat& s) {
    return {static_cast<uint64_t>(s.st_dev),static_cast<uint64_t>(s.st_ino),
        static_cast<uint64_t>(s.st_size),s.st_mtim.tv_sec,s.st_mtim.tv_nsec,true};
}
std::string Error(const char* action) { return std::string(action)+": "+strerror(errno); }
#ifdef __HAIKU__
bool CopyAttributes(int source, int target) {
    DIR* attrs=fs_fopen_attr_dir(source);
    if (!attrs) return false;
    bool ok=true;
    while (auto* entry=fs_read_attr_dir(attrs)) {
        attr_info info;
        if (fs_stat_attr(source,entry->d_name,&info)!=0) { ok=false; break; }
        char buffer[65536];
        off_t offset=0;
        while (offset<info.size) {
            ssize_t n=fs_read_attr(source,entry->d_name,info.type,offset,buffer,
                std::min<off_t>(sizeof(buffer),info.size-offset));
            if (n<=0 || fs_write_attr(target,entry->d_name,info.type,offset,buffer,n)!=n) { ok=false; break; }
            offset+=n;
        }
        if (!ok) break;
    }
    fs_close_attr_dir(attrs);
    return ok;
}
#endif
}
bool FileStamp::operator==(const FileStamp& o) const {
    return exists==o.exists && (!exists || (device==o.device && inode==o.inode && size==o.size
        && seconds==o.seconds && nanoseconds==o.nanoseconds));
}
FileStamp StatFile(const std::string& path) { struct stat s; return stat(path.c_str(),&s)==0 ? Stamp(s) : FileStamp{}; }
std::string CanonicalPath(const std::string& path) {
    std::error_code error;
    auto resolved=std::filesystem::weakly_canonical(std::filesystem::absolute(path,error),error);
    return error ? path : resolved.string();
}
bool IsValidUTF8(std::string_view bytes) {
    UTF8Validator validator;validator.Feed(bytes);return validator.Complete();
}
FileData ReadFile(const std::string& path, const std::atomic<bool>* cancel, size_t limit,
    const std::function<bool(std::string_view)>& sink) {
    FileData data;
    int fd=open(path.c_str(),O_RDONLY|O_CLOEXEC);
    if (fd<0) { data.error=Error("Cannot open file");return data; }
    struct stat s{};
    if (fstat(fd,&s)!=0 || !S_ISREG(s.st_mode)) { data.error="This is not a regular file.";close(fd);return data; }
    data.stamp=Stamp(s);
    // Most binary formats identify themselves immediately. Keep their preview
    // bounded even when the file is much larger than the text editing limit.
    char sniff[65536];
    ssize_t sniffSize=pread(fd,sniff,sizeof(sniff),0);
    if(sniffSize>0 && std::find(sniff,sniff+sniffSize,'\0')!=sniff+sniffSize) {
        data.binary=true;data.utf8=IsValidUTF8(std::string_view(sniff,sniffSize));
        data.bytes.assign(sniff,sniffSize);
        struct stat after{};if(fstat(fd,&after)!=0 || Stamp(after)!=data.stamp) data.error="File changed while reading. Open it again.";
        close(fd);return data;
    }
    if (data.stamp.size>limit) { data.error="The file exceeds the current 1 GiB editing limit.";close(fd);return data; }
    std::string block;
    try { if(sink) { block.resize(1024*1024);if(sniffSize>0) data.bytes.assign(sniff,sniffSize); } else data.bytes.resize(data.stamp.size); }
    catch (...) { data.error="Not enough memory to open this file.";close(fd);return data; }
    size_t offset=0;
    UTF8Validator validator;bool foundEOL=false,pendingCR=false;
    data.bom=sniffSize>=3 && std::string_view(sniff,3)=="\xef\xbb\xbf";
    while (offset<data.stamp.size) {
        if (cancel && cancel->load()) { data.error="Open cancelled.";break; }
        char* destination=sink?block.data():data.bytes.data()+offset;
        ssize_t n=read(fd,destination,std::min<uint64_t>(1024*1024,data.stamp.size-offset));
        if (n>0) {
            std::string_view chunk(destination,n);validator.Feed(chunk);
            data.binary=data.binary || chunk.find('\0')!=std::string_view::npos;
            if(pendingCR) { data.eol=chunk.front()=='\n'?0:1;pendingCR=false; }
            if(!foundEOL) {
                auto eol=chunk.find_first_of("\r\n");
                if(eol!=std::string_view::npos) { foundEOL=true;
                    if(chunk[eol]=='\r') { data.eol=1;pendingCR=eol+1==chunk.size();if(!pendingCR && chunk[eol+1]=='\n') data.eol=0; }
                }
            }
            if(sink) {
                auto content=chunk;if(offset==0 && data.bom) content.remove_prefix(3);
                if(!sink(content)) { data.error="Not enough memory to load this document.";break; }
            }
            offset+=n;
        }
        else if (n<0 && errno==EINTR) continue;
        else { data.error=n==0 ? "File changed while reading. Open it again." : Error("Cannot read file");break; }
    }
    struct stat after{};
    if (fstat(fd,&after)!=0 || Stamp(after)!=data.stamp) data.error="File changed while reading. Open it again.";
    close(fd);
    if (!data.ok()) { data.bytes.clear();return data; }
    data.utf8=validator.Complete();
    return data;
}
std::string SaveFile(const std::string& inputPath, std::string_view bytes, const FileStamp& expected) {
    return SaveFileParts(inputPath,{bytes},expected);
}
std::string SaveFileParts(const std::string& inputPath,const std::vector<std::string_view>& parts,const FileStamp& expected) {
    std::string path=CanonicalPath(inputPath);
    if (StatFile(path)!=expected) return "The file changed on disk. Reload it or save your edits to a different file.";
    int old=-1;
    struct stat oldStat{};
    if (expected.exists) {
        old=open(path.c_str(),O_RDONLY|O_CLOEXEC);
        if (old<0 || fstat(old,&oldStat)!=0) { if(old>=0) close(old);return Error("Cannot inspect original file"); }
        if (!(oldStat.st_mode&0222)) { close(old);return "The file is read-only. Save to a different file."; }
    }
    std::string name=path+".kiri-save-XXXXXX";
    int fd=mkstemp(name.data());
    if (fd<0) { if(old>=0) close(old);return Error("Cannot create temporary file"); }
    fcntl(fd,F_SETFD,FD_CLOEXEC);
    std::string error;
    if (old>=0 && fchmod(fd,oldStat.st_mode&0777)!=0) error=Error("Cannot preserve file permissions");
    // New documents retain mkstemp's private 0600 permissions. Existing files
    // keep their own permissions, including their executable bit.
#ifdef __HAIKU__
    if (old>=0 && error.empty() && !CopyAttributes(old,fd)) error=Error("Cannot preserve file attributes");
#endif
    for(auto bytes:parts) {
      size_t offset=0;
      while (error.empty() && offset<bytes.size()) {
        ssize_t n=write(fd,bytes.data()+offset,std::min<size_t>(1024*1024,bytes.size()-offset));
        if (n>0) offset+=n;
        else if (n<0 && errno==EINTR) continue;
        else error=Error("Cannot write file");
      }
    }
    if (error.empty() && fsync(fd)!=0) error=Error("Cannot flush file");
    if (close(fd)!=0 && error.empty()) error=Error("Cannot close file");
    if (error.empty() && StatFile(path)!=expected) error="The file changed during save. Your edits are still in the editor.";
    if (error.empty() && rename(name.c_str(),path.c_str())!=0) error=Error("Cannot replace file");
    if (!error.empty()) unlink(name.c_str());
    if (old>=0) close(old);
    if (error.empty()) {
        auto parent=std::filesystem::path(path).parent_path().string();
        int directory=open(parent.c_str(),O_RDONLY|O_CLOEXEC);
        if (directory>=0) { fsync(directory);close(directory); }
    }
    return error;
}
std::string HexPreview(std::string_view bytes, uint64_t totalSize) {
    std::string result="Binary file — "+std::to_string(totalSize)+" bytes\n\n";
    const size_t limit=std::min<size_t>(bytes.size(),65536);
    char buffer[32];
    for (size_t offset=0;offset<limit;offset+=16) {
        snprintf(buffer,sizeof(buffer),"%08llx  ",static_cast<unsigned long long>(offset));result+=buffer;
        for(size_t i=0;i<16;++i) {
            if(offset+i<limit) snprintf(buffer,sizeof(buffer),"%02x ",static_cast<unsigned char>(bytes[offset+i]));
            else strcpy(buffer,"   ");
            result+=buffer;if(i==7) result+=' ';
        }
        result+=" |";
        for(size_t i=0;i<16 && offset+i<limit;++i) {
            unsigned char c=bytes[offset+i];result+=c>=32 && c<127 ? static_cast<char>(c) : '.';
        }
        result+="|\n";
    }
    if(totalSize>limit) result+="\nPreview limited to the first 64 KiB.\n";
    return result;
}
}
