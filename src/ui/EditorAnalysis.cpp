#include "ui/Editor.h"
#include <algorithm>

namespace kiri {
void Editor::NoteInput() {
    ++fState->revision;++fState->inputRevision;ClearAnalysis();
}
void Editor::ClearAnalysis() {
    for(int indicator=9;indicator<=24;++indicator) {
        if(indicator>=18 && indicator<=20) continue; // Find uses 8; diff ranges use 20.
        SendMessage(SCI_SETINDICATORCURRENT,indicator);SendMessage(SCI_INDICATORCLEARRANGE,0,SendMessage(SCI_GETLENGTH));
    }
    fState->diagnostics.clear();fState->semanticTokens.clear();SendMessage(SCI_CALLTIPCANCEL);
}
void Editor::SetDiagnostics(const std::vector<Diagnostic>& diagnostics) {
    auto length=SendMessage(SCI_GETLENGTH);
    for(int i=21;i<25;++i) {SendMessage(SCI_SETINDICATORCURRENT,i);SendMessage(SCI_INDICATORCLEARRANGE,0,length);}
    fState->diagnostics=diagnostics;
    for(auto& item:diagnostics) {
        auto start=std::min<sptr_t>(item.start,length),end=std::min<sptr_t>(item.end,length);
        if(start==end && length) {if(end<length) end=SendMessage(SCI_POSITIONAFTER,end);else start=SendMessage(SCI_POSITIONBEFORE,start);}
        SendMessage(SCI_SETINDICATORCURRENT,20+std::clamp(item.severity,1,4));SendMessage(SCI_INDICATORFILLRANGE,start,end-start);
    }
}
void Editor::SetSemanticTokens(const std::vector<SemanticToken>& tokens) {
    for(int i=9;i<=17;++i) {SendMessage(SCI_SETINDICATORCURRENT,i);SendMessage(SCI_INDICATORCLEARRANGE,0,SendMessage(SCI_GETLENGTH));}
    fState->semanticTokens=tokens;
    for(auto& token:tokens) {
        SendMessage(SCI_SETINDICATORCURRENT,9+int(token.role));SendMessage(SCI_INDICATORFILLRANGE,token.start,token.end-token.start);
        if(token.deprecated) {SendMessage(SCI_SETINDICATORCURRENT,17);SendMessage(SCI_INDICATORFILLRANGE,token.start,token.end-token.start);}
    }
}
void Editor::ShowDiagnostic(size_t position) {
    std::string message;
    for(auto& item:fState->diagnostics) if(position>=item.start && position<=item.end) {
        if(!message.empty()) message+="\n\n";
        message+=SeverityName(item.severity);if(!item.source.empty()) message+=" · "+item.source;if(!item.code.empty()) message+=" "+item.code;
        message+="\n"+item.message;if(message.size()>8192) break;
    }
    if(!message.empty()) SendMessage(SCI_CALLTIPSHOW,position,reinterpret_cast<sptr_t>(message.c_str()));
}
}
