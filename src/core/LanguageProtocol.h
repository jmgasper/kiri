#pragma once
#include <nlohmann/json.hpp>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace kiri {
using Json=nlohmann::json;
enum class PositionEncoding { UTF16,UTF8,UTF32 };
struct TextPosition { size_t line=0,character=0; };
struct TextEdit { size_t start=0,end=0;std::string text; };
struct DocumentSymbol {
    std::string name,container,detail;
    int kind=0,depth=0;
    size_t start=0,end=0,selection=0;
};
struct CompletionItem {
    std::string label,detail,filter,sort;
    Json value;
};
TextPosition PositionAt(std::string_view text,size_t offset,PositionEncoding encoding=PositionEncoding::UTF16);
size_t OffsetAt(std::string_view text,TextPosition position,PositionEncoding encoding=PositionEncoding::UTF16);
Json PositionJSON(TextPosition position);
TextPosition ReadPosition(const Json& position);
std::string FileURI(const std::string& path);
std::vector<DocumentSymbol> ReadSymbols(const Json& result,std::string_view text,const std::string& uri,PositionEncoding encoding);
std::vector<CompletionItem> ReadCompletions(const Json& result);
std::vector<TextEdit> CompletionEdits(const CompletionItem& item,std::string_view text,size_t wordStart,size_t caret,PositionEncoding encoding);
// Validates all ranges before callers modify an editor. Returned order is descending.
std::vector<TextEdit> OrderedEdits(std::vector<TextEdit> edits,size_t length);
size_t MapOffset(size_t offset,const std::vector<TextEdit>& edits);
std::string ApplyTextEdits(std::string text,const std::vector<TextEdit>& edits);
const char* SymbolKindName(int kind);
// Incremental byte framing, shared by the actual transport and protocol tests.
class RpcFramer {
public:
    static constexpr size_t Limit=16*1024*1024;
    std::vector<Json> Feed(std::string_view bytes);
    static std::string Frame(const Json& value);
private:
    std::string fBuffer;
    size_t fLength=0;
    bool fHeader=true;
};
}
