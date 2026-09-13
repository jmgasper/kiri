#include "core/Recovery.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace kiri {
namespace {
constexpr uint64_t kHashStart=14695981039346656037ULL;
uint64_t Hash(std::string_view data,uint64_t hash=kHashStart) {
    for(unsigned char byte:data) { hash^=byte;hash*=1099511628211ULL; }return hash;
}
void Number(std::string& data,uint64_t value) { for(int i=0;i<8;++i) { data+=static_cast<char>(value&255);value>>=8; } }
void String(std::string& data,const std::string& value) { Number(data,value.size());data+=value; }
class Reader {
public:
    explicit Reader(std::string_view input):data(input) {}
    uint64_t Number() {
        if(data.size()<8) throw std::runtime_error("Incomplete recovery header.");
        uint64_t value=0;for(int i=0;i<8;++i) value|=uint64_t(static_cast<unsigned char>(data[i]))<<(i*8);
        data.remove_prefix(8);return value;
    }
    std::string String() {
        auto size=Number();if(size>data.size()) throw std::runtime_error("Invalid recovery metadata.");
        std::string value(data.substr(0,size));data.remove_prefix(size);return value;
    }
private:std::string_view data;
};
}
std::string WriteDraft(const std::string& file,const Draft& d) {
    std::string header;String(header,d.path);String(header,d.name);
    for(uint64_t value:{d.base.device,d.base.inode,d.base.size,uint64_t(d.base.seconds),uint64_t(d.base.nanoseconds),
        uint64_t(d.base.exists),d.caret,d.anchor,d.firstLine,uint64_t(d.bom),uint64_t(d.eol),uint64_t(d.text.size()),Hash(d.text)}) Number(header,value);
    if(header.size()>65536) return "Recovery metadata is too large.";
    std::string prefix="Kiridrf1";Number(prefix,header.size());
    return SaveFileParts(file,{prefix,header,d.text},StatFile(file));
}
Draft ReadDraft(const std::string& file,const std::function<bool(std::string_view)>& sink) {
    Draft d;
    try {
        std::ifstream input(file,std::ios::binary);char prefix[16]{};input.read(prefix,sizeof(prefix));
        if(input.gcount()!=sizeof(prefix) || std::string_view(prefix,8)!="Kiridrf1") throw std::runtime_error("Unrecognized recovery file.");
        auto headerSize=Reader(std::string_view(prefix+8,8)).Number();
        if(headerSize>65536) throw std::runtime_error("Invalid recovery header size.");
        std::string header(headerSize,'\0');input.read(header.data(),header.size());
        if(size_t(input.gcount())!=headerSize) throw std::runtime_error("Incomplete recovery file.");
        Reader read(header);d.path=read.String();d.name=read.String();
        d.base.device=read.Number();d.base.inode=read.Number();d.base.size=read.Number();d.base.seconds=read.Number();d.base.nanoseconds=read.Number();d.base.exists=read.Number();
        d.caret=read.Number();d.anchor=read.Number();d.firstLine=read.Number();d.bom=read.Number();d.eol=read.Number();
        auto size=read.Number(),expectedHash=read.Number();
        if(size>1024ULL*1024*1024 || d.eol<0 || d.eol>2 || StatFile(file).size!=16+headerSize+size)
            throw std::runtime_error("Invalid recovery document size or format.");
        if(!sink) d.text.reserve(size);
        char buffer[65536];uint64_t remaining=size,hash=kHashStart;
        while(remaining) {
            auto count=std::min<uint64_t>(sizeof(buffer),remaining);input.read(buffer,count);
            if(uint64_t(input.gcount())!=count) throw std::runtime_error("Incomplete recovery contents.");
            std::string_view chunk(buffer,count);hash=Hash(chunk,hash);
            if(sink) { if(!sink(chunk)) throw std::runtime_error("Cannot load recovery contents."); }
            else d.text.append(chunk);
            remaining-=count;
        }
        if(hash!=expectedHash) throw std::runtime_error("Recovery checksum mismatch; the file has been kept for inspection.");
    } catch(const std::exception& error) { d.error=error.what();d.text.clear(); }
    return d;
}
std::vector<std::string> ListDrafts(const std::string& directory) {
    std::vector<std::string> files;std::error_code error;
    for(std::filesystem::directory_iterator it(directory,error),end;!error && it!=end;it.increment(error))
        if(it->path().extension()==".draft" && it->is_regular_file(error) && !it->is_symlink(error)) files.push_back(it->path().string());
    std::sort(files.begin(),files.end());return files;
}
}
