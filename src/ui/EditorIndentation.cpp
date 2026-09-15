#include "ui/Editor.h"
#include "ui/MinimapView.h"
#include <Clipboard.h>
#include <Message.h>
#include <algorithm>
#include <numeric>

namespace kiri {
namespace {
std::string Indent(int columns,const Indentation& settings) {
    columns=std::clamp(columns,0,1024*1024);
    return settings.tabs?std::string(columns/settings.tabWidth,'\t')+std::string(columns%settings.tabWidth,' '):std::string(columns,' ');
}
int Columns(std::string_view text,int tabWidth) {int column=0;for(char c:text) {if(c==' ') ++column;else if(c=='\t') column+=tabWidth-column%tabWidth;else break;}return column;}
std::string EOL(Editor& editor) {auto mode=editor.SendMessage(SCI_GETEOLMODE);return mode==SC_EOL_CRLF?"\r\n":mode==SC_EOL_CR?"\r":"\n";}
}
DocumentStyle Editor::EffectiveStyle() const {return ResolveDocumentStyle(fSettings.indentation,fSettings.guideColumns,fState->config,fState->overrides);}
void Editor::ApplyDocumentStyle() {
    auto style=EffectiveStyle();
    SendMessage(SCI_SETUSETABS,style.indentation.tabs);SendMessage(SCI_SETTABWIDTH,style.indentation.tabWidth);SendMessage(SCI_SETINDENT,style.indentation.indentWidth);
    SendMessage(SCI_MULTIEDGECLEARALL);
    for(int column:style.guides) SendMessage(SCI_MULTIEDGEADDLINE,column,SciColor(fTheme.border));
    SendMessage(SCI_SETEDGEMODE,style.guides.empty()?EDGE_NONE:EDGE_MULTILINE);
    if(fMinimap) fMinimap->InvalidateContent();
}
bool Editor::FilterEditingInput(BMessage* message) {
    if(message->what==B_PASTE) return PasteClipboard();
    if(message->what!=B_KEY_DOWN) return false;
    const char* bytes=nullptr;int32 modifiers=0;message->FindInt32("modifiers",&modifiers);
    if(message->FindString("bytes",&bytes)!=B_OK || !bytes || !*bytes || (modifiers&(B_COMMAND_KEY|B_CONTROL_KEY|B_OPTION_KEY))) return false;
    if(bytes[0]==B_ENTER && !SendMessage(SCI_AUTOCACTIVE)) {InsertNewline();return true;}
    return false;
}
void Editor::ReplaceSelections(const std::vector<std::string>& replacements) {
    if(SendMessage(SCI_GETREADONLY)) return;
    struct Selection {sptr_t start,end,caret;size_t index;};std::vector<Selection> selections;
    auto count=SendMessage(SCI_GETSELECTIONS),main=SendMessage(SCI_GETMAINSELECTION);
    if(replacements.size()!=size_t(count)) return;
    for(sptr_t i=0;i<count;++i) selections.push_back({SendMessage(SCI_GETSELECTIONNSTART,i),SendMessage(SCI_GETSELECTIONNEND,i),0,size_t(i)});
    std::sort(selections.begin(),selections.end(),[](const auto& a,const auto& b){return a.start<b.start;});
    sptr_t delta=0;for(auto& selection:selections) {selection.caret=selection.start+delta+replacements[selection.index].size();delta+=sptr_t(replacements[selection.index].size())-(selection.end-selection.start);}
    for(size_t i=1;i<selections.size();++i) if(selections[i].start<selections[i-1].end) return;
    SendMessage(SCI_BEGINUNDOACTION);
    for(auto it=selections.rbegin();it!=selections.rend();++it) {const auto& text=replacements[it->index];SendMessage(SCI_SETTARGETRANGE,it->start,it->end);SendMessage(SCI_REPLACETARGET,text.size(),reinterpret_cast<sptr_t>(text.data()));}
    std::sort(selections.begin(),selections.end(),[](const auto& a,const auto& b){return a.index<b.index;});
    SendMessage(SCI_CLEARSELECTIONS);
    for(size_t i=0;i<selections.size();++i) {auto caret=selections[i].caret;if(i==0) SendMessage(SCI_SETSEL,caret,caret);else SendMessage(SCI_ADDSELECTION,caret,caret);}
    SendMessage(SCI_SETMAINSELECTION,main);SendMessage(SCI_ENDUNDOACTION);SendMessage(SCI_SCROLLCARET);
}
void Editor::InsertNewline() {
    if(SendMessage(SCI_GETREADONLY)) return;
    auto settings=EffectiveStyle().indentation;std::vector<std::string> replacements;auto eol=EOL(*this);
    for(sptr_t i=0;i<SendMessage(SCI_GETSELECTIONS);++i) {
        auto start=SendMessage(SCI_GETSELECTIONNSTART,i),line=SendMessage(SCI_LINEFROMPOSITION,start);
        auto columns=std::min(SendMessage(SCI_GETLINEINDENTATION,line),SendMessage(SCI_GETCOLUMN,start));
        replacements.push_back(eol+Indent(columns,settings));
    }
    ReplaceSelections(replacements);
}
bool Editor::PasteClipboard() {
    // Let Scintilla retain its native column and multiple-selection paste
    // behavior. Reindentation applies to a single ordinary insertion only.
    if(SendMessage(SCI_GETSELECTIONS)!=1 || SendMessage(SCI_SELECTIONISRECTANGLE)) return false;
    if(!be_clipboard || !be_clipboard->Lock()) return false;
    const void* bytes=nullptr;ssize_t count=0;auto* data=be_clipboard->Data();bool found=data && data->FindData("text/plain",B_MIME_TYPE,&bytes,&count)==B_OK && count>=0;
    std::string text;if(found) text.assign(static_cast<const char*>(bytes),count);be_clipboard->Unlock();
    if(found) PasteText(text);return found;
}
void Editor::PasteText(const std::string& input) {
    if(SendMessage(SCI_GETREADONLY)) return;
    auto settings=EffectiveStyle().indentation;auto eol=EOL(*this);std::string text;text.reserve(input.size());
    // Match Scintilla's normal paste-ending conversion without touching any
    // existing line endings in the document.
    for(size_t i=0;i<input.size();++i) {if(input[i]=='\r' || input[i]=='\n') {if(input[i]=='\r' && i+1<input.size() && input[i+1]=='\n') ++i;text+=eol;}else text+=input[i];}
    std::vector<std::string> replacements(SendMessage(SCI_GETSELECTIONS),text);
    // Reindent only a multiline paste in a line's leading whitespace. Inline
    // pastes and large pastes preserve their indentation. Relative columns in
    // the clipboard are retained, avoiding guessed source indentation widths.
    if(input.size()<=8*1024*1024 && input.find_first_of("\r\n")!=std::string::npos) {
        std::vector<std::string_view> lines;size_t start=0;
        for(size_t i=0;i<=text.size();++i) if(i==text.size() || text.compare(i,eol.size(),eol)==0) {lines.emplace_back(text.data()+start,i-start);if(i<text.size()) i+=eol.size()-1;start=i+1;}
        int base=INT32_MAX;for(auto line:lines) if(line.find_first_not_of(" \t")!=std::string_view::npos) base=std::min(base,Columns(line,settings.tabWidth));
        if(base==INT32_MAX) base=0;
        for(size_t selection=0;selection<replacements.size();++selection) {
            auto position=SendMessage(SCI_GETSELECTIONNSTART,selection),line=SendMessage(SCI_LINEFROMPOSITION,position);
            if(position>SendMessage(SCI_GETLINEINDENTPOSITION,line)) continue;
            int destination=SendMessage(SCI_GETCOLUMN,position);std::string replacement;
            for(size_t i=0;i<lines.size();++i) {if(i) replacement+=eol;auto content=lines[i].find_first_not_of(" \t");if(content==std::string_view::npos) continue;
                int columns=std::max(0,Columns(lines[i],settings.tabWidth)-base)+(i?destination:0);replacement+=Indent(columns,settings);replacement+=lines[i].substr(content);}
            replacements[selection]=std::move(replacement);
        }
    }
    ReplaceSelections(replacements);
}
}
