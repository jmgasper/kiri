#pragma once
#include "core/LanguageProtocol.h"
#include <cstdint>

namespace kiri {
constexpr size_t kLanguageBytes=8*1024*1024,kDiagnosticLimit=2000,kSemanticLimit=250000;
enum class SemanticRole { Type,Parameter,Property,Function,Variable,Namespace,Readonly,Keyword };
struct SemanticToken { size_t start=0,end=0;SemanticRole role=SemanticRole::Variable;bool deprecated=false; };
struct Diagnostic {
    size_t start=0,end=0,line=0,column=0;
    int severity=1;
    std::string message,source,code;
};
struct DiagnosticResult { std::vector<Diagnostic> items;std::string warning; };
struct SemanticResult { std::vector<SemanticToken> tokens;std::string error; };
// Index once, then map each protocol position in logarithmic time. Positions
// inside a UTF-8 character, UTF-16 surrogate pair, or outside a line are invalid.
class SourcePositions {
public:
    SourcePositions(std::string_view text,PositionEncoding encoding);
    size_t Offset(TextPosition position) const;
private:
    struct Line { uint32_t byte,unit; };
    struct Character { uint32_t byte,unit;uint8_t bytes,units; };
    std::string_view fText;
    PositionEncoding fEncoding;
    std::vector<Line> fLines;
    std::vector<Character> fCharacters;
};
DiagnosticResult ReadDiagnostics(const Json& rows,std::string_view text,PositionEncoding encoding);
SemanticResult ReadSemanticTokens(const Json& result,const Json& legend,std::string_view text,PositionEncoding encoding);
const char* SeverityName(int severity);
const char* SemanticColorRole(SemanticRole role);
}
