#include "ui/Editor.h"
#include "ui/MinimapView.h"
#include <ScrollBar.h>
#include "ui/Messages.h"
#include <ILexer.h>
#include <Lexilla.h>
#include <SciLexer.h>
#include <Font.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>
#include <MessageFilter.h>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>

namespace kiri {
namespace {
class EditInputFilter:public BMessageFilter {
public:
    explicit EditInputFilter(Editor* editor):BMessageFilter(B_ANY_DELIVERY,B_ANY_SOURCE),fEditor(editor) {}
    filter_result Filter(BMessage* message,BHandler**) override {
        if(fEditor->FilterLanguageKey(message)) return B_SKIP_MESSAGE;
        if(fEditor->FilterEditingInput(message)) return B_SKIP_MESSAGE;
        switch(message->what) {
            case B_KEY_DOWN: {
                const char* bytes=nullptr;int32 modifiers=0;message->FindInt32("modifiers",&modifiers);
                if(message->FindString("bytes",&bytes)==B_OK && bytes && *bytes) {
                    auto c=static_cast<unsigned char>(*bytes);
                    if(c==B_BACKSPACE || c==B_DELETE || c==B_ENTER || c==B_TAB || (c>=32 && !(modifiers&(B_COMMAND_KEY|B_CONTROL_KEY)))) fEditor->NoteInput();
                }
                break;
            }
            case B_INPUT_METHOD_EVENT:
            case B_CUT:case B_PASTE:case B_UNDO:case B_REDO:case B_SIMPLE_DATA:fEditor->NoteInput();break;
            default:if(message->WasDropped()) fEditor->NoteInput();break;
        }
        return B_DISPATCH_MESSAGE;
    }
private:Editor* fEditor;
};
}
Editor::Editor():BScintillaView("source",B_FRAME_EVENTS,true,true,B_NO_BORDER) {
    SetExplicitMinSize(BSize(80,60));
    SendMessage(SCI_SETCODEPAGE,SC_CP_UTF8);
    SendMessage(SCI_SETMARGINTYPEN,0,SC_MARGIN_NUMBER);
    SendMessage(SCI_SETMARGINWIDTHN,0,54);
    SendMessage(SCI_SETMARGINTYPEN,1,SC_MARGIN_SYMBOL);
    SendMessage(SCI_SETMARGINMASKN,1,SC_MASK_FOLDERS);
    SendMessage(SCI_SETMARGINSENSITIVEN,1,1);
    SendMessage(SCI_SETMARGINWIDTHN,1,16);
    SendMessage(SCI_SETMARGINWIDTHN,2,0);
    SendMessage(SCI_SETMARGINLEFT,0,8);SendMessage(SCI_SETMARGINRIGHT,0,12);
    SendMessage(SCI_MARKERDEFINE,SC_MARKNUM_FOLDER,SC_MARK_BOXPLUS);
    SendMessage(SCI_MARKERDEFINE,SC_MARKNUM_FOLDEROPEN,SC_MARK_BOXMINUS);
    SendMessage(SCI_MARKERDEFINE,SC_MARKNUM_FOLDEREND,SC_MARK_BOXPLUSCONNECTED);
    SendMessage(SCI_MARKERDEFINE,SC_MARKNUM_FOLDEROPENMID,SC_MARK_BOXMINUSCONNECTED);
    SendMessage(SCI_MARKERDEFINE,SC_MARKNUM_FOLDERMIDTAIL,SC_MARK_TCORNER);
    SendMessage(SCI_MARKERDEFINE,SC_MARKNUM_FOLDERSUB,SC_MARK_VLINE);
    SendMessage(SCI_MARKERDEFINE,SC_MARKNUM_FOLDERTAIL,SC_MARK_LCORNER);
    SendMessage(SCI_SETMULTIPLESELECTION,1);SendMessage(SCI_SETADDITIONALSELECTIONTYPING,1);
    SendMessage(SCI_SETTABWIDTH,4);SendMessage(SCI_SETINDENT,4);SendMessage(SCI_SETUSETABS,0);
    SendMessage(SCI_SETBACKSPACEUNINDENTS,1);SendMessage(SCI_SETTABINDENTS,1);
    SendMessage(SCI_SETSCROLLWIDTH,1);SendMessage(SCI_SETSCROLLWIDTHTRACKING,1);
    SendMessage(SCI_SETENDATLASTLINE,0);SendMessage(SCI_SETLAYOUTCACHE,SC_CACHE_PAGE);
    SendMessage(SCI_SETIDLESTYLING,SC_IDLESTYLING_ALL);
    SendMessage(SCI_SETCARETWIDTH,2);SendMessage(SCI_SETCARETLINEVISIBLE,1);
    SendMessage(SCI_SETINDENTATIONGUIDES,SC_IV_LOOKBOTH);
    SendMessage(SCI_USEPOPUP,0);
    SendMessage(SCI_SETMOUSEDWELLTIME,500);
    ApplyTheme(fTheme);
    SendMessage(SCI_AUTOCSETSEPARATOR,'\n');SendMessage(SCI_AUTOCSETTYPESEPARATOR,31);SendMessage(SCI_AUTOCSETORDER,SC_ORDER_CUSTOM);
    SendMessage(SCI_AUTOCSETMAXHEIGHT,10);SendMessage(SCI_AUTOCSETMAXWIDTH,80);
    SendMessage(SCI_AUTOCSETCHOOSESINGLE,0);SendMessage(SCI_AUTOCSETAUTOHIDE,0);
    Target()->AddFilter(new EditInputFilter(this));
    fMinimap=new MinimapView(this);AddChild(fMinimap);
}
void Editor::AllAttached() {
    BScintillaView::AllAttached();
    // The Haiku port may mark styles valid before it has a drawing surface.
    // Realize fonts again once its inner view has attached to the window.
    ApplyTheme(fTheme);
    LayoutMinimap();
}
void Editor::FrameResized(float width,float height) {BView::FrameResized(width,height);LayoutMinimap();}
void Editor::DoLayout() {if(fMinimap) LayoutMinimap();else BScintillaView::DoLayout();}
void Editor::LayoutMinimap() {
    if(!fMinimap || !Window() || fLayingOutMinimap) return;fLayingOutMinimap=true;
    auto* horizontal=ScrollBar(B_HORIZONTAL);auto* vertical=ScrollBar(B_VERTICAL);
    // Own this borderless scroll view's layout. Calling BScrollView::DoLayout
    // and then shrinking its target repeatedly invalidates Haiku's layout and
    // prevents the window from painting when the minimap is enabled.
    float barWidth=vertical?B_V_SCROLL_BAR_WIDTH:0,barHeight=horizontal?B_H_SCROLL_BAR_HEIGHT:0;
    auto bounds=Bounds();float right=bounds.right-barWidth-(vertical?1:0),bottom=std::max(1.f,bounds.bottom-barHeight-(horizontal?1:0));
    auto content=std::max(1.f,right-(fSettings.minimap?96:0));
    Target()->MoveTo(0,0);Target()->ResizeTo(content,bottom);
    if(horizontal) {horizontal->MoveTo(0,bottom+1);horizontal->ResizeTo(content,barHeight);}
    if(vertical) {vertical->MoveTo(right+1,0);vertical->ResizeTo(barWidth,bottom);}
    fMinimap->MoveTo(content+1,0);fMinimap->ResizeTo(95,bottom);fMinimap->InvalidateContent();fLayingOutMinimap=false;
}
sptr_t Editor::SendMessage(unsigned int message,uptr_t wParam,sptr_t lParam) {
    if(!fLoading && message==SCI_NEWLINE) {InsertNewline();return 0;}
    if(!fLoading && message==SCI_PASTE && PasteClipboard()) return 0;
    // The Haiku port delivers SCN_MODIFIED asynchronously. Count direct edits
    // and input dispatch synchronously so a save cannot miss queued changes.
    bool edit=false;
    if(!fLoading) switch(message) {
        case SCI_ADDTEXT:case SCI_APPENDTEXT:case SCI_INSERTTEXT:case SCI_CLEARALL:
        case SCI_SETTEXT:case SCI_REPLACESEL:case SCI_REPLACETARGET:case SCI_REPLACETARGETRE:
        case SCI_DELETERANGE:case SCI_CLEAR:case SCI_CUT:case SCI_PASTE:case SCI_UNDO:case SCI_REDO:
        case SCI_SETLINEINDENTATION:case SCI_TAB:case SCI_BACKTAB:NoteInput();edit=true;break;
        default:break;
    }
    auto result=BScintillaView::SendMessage(message,wParam,lParam);
    if(fMinimap) switch(message) {case SCI_TOGGLEFOLD:case SCI_FOLDLINE:case SCI_FOLDALL:case SCI_HIDELINES:case SCI_SHOWLINES:case SCI_SETWRAPMODE:case SCI_SETZOOM:fMinimap->InvalidateContent();break;default:break;}
    if(edit && Dirty()) ++fState->changes;
    return result;
}
void Editor::Style(int id,rgb_color color,bool bold) {
    SendMessage(SCI_STYLESETFORE,id,SciColor(color));SendMessage(SCI_STYLESETBOLD,id,bold);
}
void Editor::ApplyTheme(const Theme& t) {
    fTheme=t;
    SendMessage(SCI_STYLESETFONT,STYLE_DEFAULT,reinterpret_cast<sptr_t>(fSettings.fontFamily.c_str()));
    SendMessage(SCI_STYLESETSIZE,STYLE_DEFAULT,fSettings.fontSize);
    SendMessage(SCI_STYLESETFORE,STYLE_DEFAULT,SciColor(t.text));
    SendMessage(SCI_STYLESETBACK,STYLE_DEFAULT,SciColor(t.background));
    SendMessage(SCI_STYLECLEARALL);
    SendMessage(SCI_SETSELFORE,1,SciColor(t.selectionText));SendMessage(SCI_SETSELBACK,1,SciColor(t.selection));
    for(int i=0;i<8;++i) {SendMessage(SCI_INDICSETSTYLE,9+i,INDIC_TEXTFORE);SendMessage(SCI_INDICSETFORE,9+i,SciColor(t.semantic[i]));}
    SendMessage(SCI_INDICSETSTYLE,17,INDIC_STRIKE);SendMessage(SCI_INDICSETFORE,17,SciColor(t.muted));
    for(int i=0;i<4;++i) {SendMessage(SCI_INDICSETSTYLE,21+i,INDIC_SQUIGGLE);SendMessage(SCI_INDICSETFORE,21+i,SciColor(t.diagnostic[i]));}
    SendMessage(SCI_CALLTIPSETFORE,SciColor(t.text));SendMessage(SCI_CALLTIPSETBACK,SciColor(t.panel));
    SendMessage(SCI_SETCARETFORE,SciColor(t.accent));SendMessage(SCI_SETCARETLINEBACK,SciColor(t.line));
    SendMessage(SCI_STYLESETBACK,STYLE_LINENUMBER,SciColor(t.background));Style(STYLE_LINENUMBER,t.muted);
    SendMessage(SCI_SETFOLDMARGINCOLOUR,1,SciColor(t.background));
    SendMessage(SCI_SETFOLDMARGINHICOLOUR,1,SciColor(t.background));
    for(int i=SC_MARKNUM_FOLDEREND;i<=SC_MARKNUM_FOLDEROPEN;++i) {
        SendMessage(SCI_MARKERSETFORE,i,SciColor(t.background));SendMessage(SCI_MARKERSETBACK,i,SciColor(t.muted));
    }
    Style(STYLE_BRACELIGHT,t.accent,true);SendMessage(SCI_STYLESETBACK,STYLE_BRACELIGHT,SciColor(t.selection));
    if(fLexer=="cpp") {
        for(int s:{SCE_C_COMMENT,SCE_C_COMMENTLINE,SCE_C_COMMENTDOC,SCE_C_COMMENTLINEDOC}) Style(s,t.comment);
        for(int s:{SCE_C_WORD,SCE_C_WORD2}) Style(s,t.keyword);
        for(int s:{SCE_C_STRING,SCE_C_CHARACTER,SCE_C_STRINGRAW}) Style(s,t.string);
        Style(SCE_C_NUMBER,t.number);Style(SCE_C_PREPROCESSOR,t.type);Style(SCE_C_OPERATOR,t.accent);
    } else if(fLexer=="python") {
        for(int s:{SCE_P_COMMENTLINE,SCE_P_COMMENTBLOCK}) Style(s,t.comment);
        for(int s:{SCE_P_STRING,SCE_P_CHARACTER,SCE_P_TRIPLE,SCE_P_TRIPLEDOUBLE,SCE_P_FSTRING}) Style(s,t.string);
        Style(SCE_P_WORD,t.keyword);Style(SCE_P_WORD2,t.type);Style(SCE_P_NUMBER,t.number);Style(SCE_P_DEFNAME,t.accent);Style(SCE_P_CLASSNAME,t.type);
    } else if(fLexer=="hypertext" || fLexer=="xml") {
        for(int s:{SCE_H_TAG,SCE_H_TAGUNKNOWN,SCE_H_TAGEND,SCE_H_XMLSTART,SCE_H_XMLEND}) Style(s,t.type);
        for(int s:{SCE_H_ATTRIBUTE,SCE_H_ATTRIBUTEUNKNOWN}) Style(s,t.keyword);
        for(int s:{SCE_H_DOUBLESTRING,SCE_H_SINGLESTRING,SCE_H_CDATA}) Style(s,t.string);
        Style(SCE_H_COMMENT,t.comment);Style(SCE_H_NUMBER,t.number);
        for(int s:{SCE_HJ_KEYWORD,SCE_HJ_WORD}) Style(s,t.keyword);
        for(int s:{SCE_HJ_DOUBLESTRING,SCE_HJ_SINGLESTRING}) Style(s,t.string);
        for(int s:{SCE_HJ_COMMENT,SCE_HJ_COMMENTLINE}) Style(s,t.comment);
    } else if(fLexer=="json") {
        Style(SCE_JSON_NUMBER,t.number);Style(SCE_JSON_STRING,t.string);Style(SCE_JSON_PROPERTYNAME,t.type);
        Style(SCE_JSON_KEYWORD,t.keyword);Style(SCE_JSON_OPERATOR,t.accent);Style(SCE_JSON_LINECOMMENT,t.comment);Style(SCE_JSON_BLOCKCOMMENT,t.comment);
    } else if(fLexer=="diff") {
        Style(SCE_DIFF_ADDED,t.added);Style(SCE_DIFF_DELETED,t.removed);Style(SCE_DIFF_POSITION,t.accent);
        Style(SCE_DIFF_HEADER,t.keyword,true);Style(SCE_DIFF_COMMAND,t.type,true);Style(SCE_DIFF_COMMENT,t.comment);
    } else {
        // Lexer style names are exposed by Lexilla, enabling consistent themes
        // across all installed languages without a hard-coded numeric fallback.
        int count=SendMessage(SCI_GETNAMEDSTYLES);
        for(int s=0;s<count;++s) {
            int length=SendMessage(SCI_NAMEOFSTYLE,s,0);
            if(length<=0 || length>1024) continue;
            std::string name(length+1,'\0');SendMessage(SCI_NAMEOFSTYLE,s,reinterpret_cast<sptr_t>(name.data()));
            auto has=[&](const char* word){return name.find(word)!=std::string::npos;};
            if(has("comment")) Style(s,t.comment);
            else if(has("string") || has("character")) Style(s,t.string);
            else if(has("number")) Style(s,t.number);
            else if(has("keyword") || has("word")) Style(s,t.keyword);
            else if(has("type") || has("class") || has("tag")) Style(s,t.type);
            else if(has("operator")) Style(s,t.accent);
        }
    }
    ApplyDocumentStyle();UpdateMarginWidth();if(fMinimap) fMinimap->ApplyTheme(t);Invalidate();
}
void Editor::ApplySettings(const EditorSettings& settings) {
    fSettings=settings;
    ApplyTheme(settings.Colors());
    SetExplicitMinSize(BSize(settings.minimap?180:80,60));
    if(fMinimap) {fMinimap->SetEnabled(settings.minimap);LayoutMinimap();}
}
void Editor::UpdateMarginWidth() {
    fDigits=std::to_string(SendMessage(SCI_GETLINECOUNT)).size();
    std::string digits(std::max(3,fDigits),'9');
    auto width=SendMessage(SCI_TEXTWIDTH,STYLE_LINENUMBER,reinterpret_cast<sptr_t>(digits.c_str()));
    SendMessage(SCI_SETMARGINWIDTHN,0,std::max<sptr_t>(48,width+16));
}
void Editor::SetLanguage(const std::string& path,bool large) {
    auto name=std::filesystem::path(path).filename().string();
    auto extension=std::filesystem::path(path).extension().string();
    for(auto& c:extension) c=std::tolower(static_cast<unsigned char>(c));
    static const std::map<std::string,std::pair<const char*,const char*>> languages={
        {".c",{"cpp","C"}},{".h",{"cpp","C/C++"}},{".cpp",{"cpp","C++"}},{".cc",{"cpp","C++"}},{".cxx",{"cpp","C++"}},{".hpp",{"cpp","C++"}},
        {".js",{"cpp","JavaScript"}},{".jsx",{"cpp","JavaScript JSX"}},{".ts",{"cpp","TypeScript"}},{".tsx",{"cpp","TypeScript JSX"}},{".java",{"cpp","Java"}},{".cs",{"cpp","C#"}},
        {".py",{"python","Python"}},{".pyw",{"python","Python"}},{".html",{"hypertext","HTML"}},{".htm",{"hypertext","HTML"}},{".vue",{"hypertext","Vue"}},{".svelte",{"hypertext","Svelte"}},
        {".xml",{"xml","XML"}},{".svg",{"xml","SVG"}},{".xsd",{"xml","XML"}},{".plist",{"xml","XML"}},{".json",{"json","JSON"}},{".jsonc",{"json","JSON with comments"}},
        {".css",{"css","CSS"}},{".scss",{"css","SCSS"}},{".less",{"css","Less"}},{".md",{"markdown","Markdown"}},{".markdown",{"markdown","Markdown"}},
        {".yaml",{"yaml","YAML"}},{".yml",{"yaml","YAML"}},{".toml",{"toml","TOML"}},{".ini",{"props","INI"}},{".cfg",{"props","Configuration"}},
        {".rs",{"rust","Rust"}},{".go",{"cpp","Go"}},{".sh",{"bash","Shell"}},{".bash",{"bash","Bash"}},{".zsh",{"bash","Shell"}},{".fish",{"bash","Shell"}},
        {".rb",{"ruby","Ruby"}},{".php",{"hypertext","PHP"}},{".sql",{"sql","SQL"}},{".lua",{"lua","Lua"}},{".swift",{"cpp","Swift"}},{".kt",{"cpp","Kotlin"}},
        {".cmake",{"cmake","CMake"}},{".diff",{"diff","Diff"}},{".patch",{"diff","Diff"}},{".r",{"r","R"}},{".pl",{"perl","Perl"}},{".tex",{"latex","LaTeX"}}
        ,{".dart",{"dart","Dart"}},{".zig",{"zig","Zig"}},{".jl",{"julia","Julia"}}
    };
    auto found=languages.find(extension);
    fLexer=found==languages.end()?"null":found->second.first;fLanguage=found==languages.end()?"Plain Text":found->second.second;
    if(name=="Makefile" || name=="GNUmakefile" || name=="Jamfile" || name=="Jamrules") { fLexer="makefile";fLanguage="Makefile"; }
    if(name=="CMakeLists.txt") { fLexer="cmake";fLanguage="CMake"; }
    if(name=="Dockerfile") { fLexer="bash";fLanguage="Dockerfile"; }
    if(large) { fLexer="null";fLanguage+=" · Large file"; }
    auto lexer=CreateLexer(fLexer.c_str());
    SendMessage(SCI_SETILEXER,0,reinterpret_cast<sptr_t>(lexer));
    const char* keywords="";
    if(fLexer=="cpp") keywords="alignas alignof asm auto bool break case catch char class const constexpr continue decltype default delete do double else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept nullptr operator private protected public register reinterpret_cast return short signed sizeof static static_cast struct switch template this thread_local throw true try typedef typename union unsigned using virtual void volatile wchar_t while async await let var function import from of in extends implements interface type typeof package func defer chan map range go select instanceof final null boolean byte yield val fun when object override data sealed companion init guard protocol extension self nil optional some associatedtype";
    if(fLexer=="python") keywords="and as assert async await break class continue def del elif else except False finally for from global if import in is lambda None nonlocal not or pass raise return True try while with yield match case";
    if(fLexer=="rust") keywords="as async await break const continue crate dyn else enum extern false fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait true type unsafe use where while abstract become box do final macro override priv typeof unsized virtual yield try";
    if(fLexer=="bash") keywords="if then else elif fi case esac for while do done in function select until time coproc export local return source alias unalias read echo printf cd pwd set unset trap";
    if(fLexer=="sql") keywords="select from where join left right inner outer on as and or not null insert into update delete create table alter drop index primary key foreign references values set group by order having limit offset union all distinct count sum avg min max begin commit rollback with returning";
    if(fLexer=="json") keywords="true false null";
    if(fLexer=="hypertext" || fLexer=="xml") keywords="html head body title meta link script style div span p a img ul li ol table tr td th form input button textarea section article header footer nav main h1 h2 h3 h4 h5 h6 br hr svg path g rect circle";
    if(fLexer=="ruby") keywords="alias and begin break case class def defined? do else elsif end ensure false for if in module next nil not or redo rescue retry return self super then true undef unless until when while yield BEGIN END";
    if(fLexer=="lua") keywords="and break do else elseif end false for function goto if in local nil not or repeat return then true until while";
    if(fLexer=="cmake") keywords="add_library add_executable add_subdirectory cmake_minimum_required configure_file project set unset option find_package find_library find_path target_link_libraries target_include_directories target_compile_options target_compile_definitions install file foreach endforeach if elseif else endif function endfunction macro endmacro return include message list string enable_testing add_test";
    if(fLexer=="css") keywords="align-items background background-color border border-radius box-shadow color content display flex flex-direction flex-grow font font-family font-size font-weight gap grid grid-template-columns height justify-content left line-height margin max-width min-width opacity overflow padding position right text-align text-decoration top transform transition width z-index";
    if(fLexer=="yaml") keywords="true false null yes no on off";
    if(fLexer=="perl") keywords="my our local use require package sub return if elsif else unless while until for foreach last next redo goto eval die warn print printf say undef defined bless ref exists delete push pop shift unshift split join map grep sort keys values each";
    if(fLexer=="r") keywords="if else for in while repeat function next break TRUE FALSE NULL Inf NaN NA library return source";
    if(fLexer=="dart") keywords="abstract as assert async await break case catch class const continue covariant default deferred do dynamic else enum export extends extension external factory false final finally for Function get hide if implements import in interface is late library mixin new null on operator part required rethrow return set show static super switch sync this throw true try typedef var void while with yield";
    if(fLexer=="zig") keywords="addrspace align allowzero and anyframe anytype asm async await break callconv catch comptime const continue defer else enum errdefer error export extern fn for if inline linksection noalias noinline nosuspend opaque or orelse packed pub resume return section struct suspend switch test threadlocal try union unreachable usingnamespace var volatile while";
    if(fLexer=="julia") keywords="abstract baremodule begin break catch const continue do else elseif end export false finally for function global if import let local macro module mutable primitive quote return struct true try using where while";
    SendMessage(SCI_SETKEYWORDS,0,reinterpret_cast<sptr_t>(keywords));
    if(fLexer=="hypertext" || fLexer=="xml") {
        SendMessage(SCI_SETKEYWORDS,1,reinterpret_cast<sptr_t>("async await break case catch class const continue debugger default delete do else export extends false finally for function if import in instanceof let new null of return static super switch this throw true try typeof var void while with yield"));
        SendMessage(SCI_SETKEYWORDS,4,reinterpret_cast<sptr_t>("abstract and array as break callable case catch class clone const continue declare default die do echo else elseif empty endfor endforeach endif endswitch endwhile enum eval exit extends false final finally fn for foreach function global goto if implements include include_once instanceof interface isset list match namespace new null or print private protected public readonly require require_once return static switch throw trait true try unset use var while xor yield"));
    }
    SendMessage(SCI_SETPROPERTY,reinterpret_cast<uptr_t>("fold"),reinterpret_cast<sptr_t>(large?"0":"1"));
    SendMessage(SCI_SETPROPERTY,reinterpret_cast<uptr_t>("lexer.json.allow.comments"),reinterpret_cast<sptr_t>("1"));
    SendMessage(SCI_SETPROPERTY,reinterpret_cast<uptr_t>("lexer.cpp.track.preprocessor"),reinterpret_cast<sptr_t>("0"));
    SendMessage(SCI_SETPROPERTY,reinterpret_cast<uptr_t>("lexer.css.scss.language"),reinterpret_cast<sptr_t>(extension==".scss"?"1":"0"));
    ApplyTheme(fTheme);
}
void Editor::SetText(const std::string& bytes,bool readOnly,int eol) {
    fLoading=true;SendMessage(SCI_SETREADONLY,0);SendMessage(SCI_SETUNDOCOLLECTION,0);
    SendMessage(SCI_CLEARALL);SendMessage(SCI_ALLOCATE,bytes.size());
    SendMessage(SCI_ADDTEXT,bytes.size(),reinterpret_cast<sptr_t>(bytes.data()));
    SendMessage(SCI_SETEOLMODE,eol);SendMessage(SCI_EMPTYUNDOBUFFER);SendMessage(SCI_SETUNDOCOLLECTION,1);
    SendMessage(SCI_SETSAVEPOINT);SendMessage(SCI_SETREADONLY,readOnly);fLoading=false;
}
std::shared_ptr<EditorLoader> Editor::CreateLoader(bool large) {
    auto result=std::make_shared<EditorLoader>();
    result->loader=reinterpret_cast<Scintilla::ILoader*>(SendMessage(SCI_CREATELOADER,0,
        large?SC_DOCUMENTOPTION_STYLES_NONE:SC_DOCUMENTOPTION_DEFAULT));
    return result;
}
void Editor::Adopt(EditorLoader& loader,int eol) {
    fLoading=true;auto* document=loader.Take();
    SendMessage(SCI_SETDOCPOINTER,0,reinterpret_cast<sptr_t>(document));
    SendMessage(SCI_RELEASEDOCUMENT,0,reinterpret_cast<sptr_t>(document));
    SendMessage(SCI_SETCODEPAGE,SC_CP_UTF8);SendMessage(SCI_SETEOLMODE,eol);
    SendMessage(SCI_EMPTYUNDOBUFFER);SendMessage(SCI_SETUNDOCOLLECTION,1);SendMessage(SCI_SETSAVEPOINT);
    fLoading=false;
}
void Editor::ShareDocument(Editor& source) {
    fLoading=true;
    SendMessage(SCI_SETDOCPOINTER,0,source.SendMessage(SCI_GETDOCPOINTER));
    fState=source.fState;fLexer=source.fLexer;fLanguage=source.fLanguage;
    SendMessage(SCI_SETEOLMODE,source.SendMessage(SCI_GETEOLMODE));
    SendMessage(SCI_SETREADONLY,source.SendMessage(SCI_GETREADONLY));
    fLoading=false;ApplySettings(source.fSettings);
}
std::string Editor::Text() {
    size_t size=SendMessage(SCI_GETLENGTH);std::string text(size+1,'\0');
    SendMessage(SCI_GETTEXT,size+1,reinterpret_cast<sptr_t>(text.data()));text.resize(size);return text;
}
bool Editor::Matches(std::string_view text) {
    if(size_t(SendMessage(SCI_GETLENGTH))!=text.size()) return false;
    const char* bytes=reinterpret_cast<const char*>(SendMessage(SCI_GETCHARACTERPOINTER));
    return text.empty() || (bytes && memcmp(bytes,text.data(),text.size())==0);
}
void Editor::ApplyEdits(const std::vector<TextEdit>& edits,bool preserveLines) {
    if(SendMessage(SCI_GETREADONLY) || edits.empty()) return;
    auto ordered=OrderedEdits(edits,SendMessage(SCI_GETLENGTH));
    auto caret=SendMessage(SCI_GETCURRENTPOS),anchor=SendMessage(SCI_GETANCHOR),first=SendMessage(SCI_GETFIRSTVISIBLELINE);
    auto before=preserveLines?Text():std::string();
    auto caretPosition=PositionAt(before,caret),anchorPosition=PositionAt(before,anchor);
    CancelCompletions();SendMessage(SCI_BEGINUNDOACTION);
    for(const auto& edit:ordered) {
        SendMessage(SCI_SETTARGETSTART,edit.start);SendMessage(SCI_SETTARGETEND,edit.end);
        SendMessage(SCI_REPLACETARGET,edit.text.size(),reinterpret_cast<sptr_t>(edit.text.data()));
    }
    SendMessage(SCI_ENDUNDOACTION);
    if(preserveLines) { auto after=Text();caret=OffsetAt(after,caretPosition);anchor=OffsetAt(after,anchorPosition); }
    else { caret=MapOffset(caret,edits);anchor=MapOffset(anchor,edits); }
    SendMessage(SCI_SETSEL,anchor,caret);SendMessage(SCI_SETFIRSTVISIBLELINE,first);
}
void Editor::ShowCompletions(const std::vector<std::string>& labels) {
    CancelCompletions();if(labels.empty()) return;
    fCompletionLabels=labels;std::string list;
    for(const auto& label:labels) { if(!list.empty()) list+='\n';list+=label; }
    SendMessage(SCI_USERLISTSHOW,1,reinterpret_cast<sptr_t>(list.c_str()));
}
void Editor::CancelCompletions() { SendMessage(SCI_AUTOCCANCEL);fCompletionLabels.clear(); }
bool Editor::FilterLanguageKey(BMessage* message) {
    if(message->what!=B_KEY_DOWN || !Window()) return false;
    int32 modifiers=0,raw=0;const char* bytes=nullptr;message->FindInt32("modifiers",&modifiers);message->FindInt32("raw_char",&raw);message->FindString("bytes",&bytes);
    if((raw==' ' || (bytes && bytes[0]==' ')) && (modifiers&B_CONTROL_KEY) && !(modifiers&(B_COMMAND_KEY|B_OPTION_KEY))) {
        BMessage request(kComplete);request.AddPointer("editor",this);Window()->PostMessage(&request);return true;
    }
    if(bytes && bytes[0]==B_ESCAPE) { BMessage request(kCancelCompletion);request.AddPointer("editor",this);Window()->PostMessage(&request); }
    return false;
}
void Editor::NotificationReceived(SCNotification* n) {
    if(fMinimap && n->nmhdr.code==SCN_MODIFIED && (n->modificationType&(SC_MOD_CHANGESTYLE|SC_MOD_CHANGEFOLD))) fMinimap->InvalidateContent();
    if(!fLoading && n->nmhdr.code==SCN_MODIFIED && (n->modificationType&(SC_MOD_INSERTTEXT|SC_MOD_DELETETEXT))) { ++fState->revision;++fState->changes; }
    if(fLoading || !Window()) return;
    if(n->nmhdr.code==SCN_DWELLSTART && n->position>=0) ShowDiagnostic(n->position);
    if(n->nmhdr.code==SCN_DWELLEND) SendMessage(SCI_CALLTIPCANCEL);
    if(n->nmhdr.code==SCN_FOCUSIN) { BMessage message(kEditorFocus);message.AddPointer("editor",this);Window()->PostMessage(&message); }
    if(n->nmhdr.code==SCN_MODIFIED && (n->modificationType&(SC_MOD_INSERTTEXT|SC_MOD_DELETETEXT))) {
        BMessage message(kEditorText);message.AddPointer("editor",this);Window()->PostMessage(&message);
    }
    if(n->nmhdr.code==SCN_CHARADDED) {
        BMessage message(kEditorTyped);message.AddPointer("editor",this);message.AddInt32("character",n->ch);Window()->PostMessage(&message);
    }
    if(n->nmhdr.code==SCN_USERLISTSELECTION && n->listType==1) {
        BMessage message(kCompletionChosen);message.AddPointer("editor",this);
        // The Haiku port copies notification text as bytes without a NUL.
        const void* bytes=nullptr;ssize_t size=0;auto* current=Window()->CurrentMessage();
        if(current && current->FindData("notification_text",B_ANY_TYPE,&bytes,&size)==B_OK && size>0) {
            std::string label(static_cast<const char*>(bytes),size);message.AddString("label",label.c_str());Window()->PostMessage(&message);
        }
    }
    if(n->nmhdr.code==SCN_SAVEPOINTLEFT || n->nmhdr.code==SCN_SAVEPOINTREACHED) {
        BMessage msg(kEditorChanged);msg.AddPointer("editor",this);Window()->PostMessage(&msg);
    } else if(n->nmhdr.code==SCN_UPDATEUI) {
        BMessage msg(kEditorPosition);msg.AddPointer("editor",this);Window()->PostMessage(&msg);
        int digits=std::to_string(SendMessage(SCI_GETLINECOUNT)).size();
        if(digits!=fDigits) UpdateMarginWidth();
        auto caret=SendMessage(SCI_GETCURRENTPOS);auto match=caret>0?SendMessage(SCI_BRACEMATCH,caret-1):-1;
        SendMessage(SCI_BRACEHIGHLIGHT,match>=0?caret-1:-1,match);
    } else if(n->nmhdr.code==SCN_ZOOM) UpdateMarginWidth();
    else if(n->nmhdr.code==SCN_MARGINCLICK && n->margin==1)
        SendMessage(SCI_TOGGLEFOLD,SendMessage(SCI_LINEFROMPOSITION,n->position));
}
void Editor::ContextMenu(BPoint where) {
    BPopUpMenu menu("Editor");
    auto* format=new BMessage(kFormatPrettier);format->AddPointer("editor",this);
    auto* formatItem=new BMenuItem("Format with Prettier",format);formatItem->SetEnabled(!SendMessage(SCI_GETREADONLY));menu.AddItem(formatItem);
    auto* completion=new BMessage(kComplete);completion->AddPointer("editor",this);
    auto* completeItem=new BMenuItem("Complete Code",completion);completeItem->SetEnabled(!SendMessage(SCI_GETREADONLY));menu.AddItem(completeItem);
    auto* rename=new BMessage(kRenameSymbol);rename->AddPointer("editor",this);
    menu.AddItem(new BMenuItem("Rename Symbol…",rename));
    menu.AddSeparatorItem();
    menu.AddItem(new BMenuItem("Copy GitHub Permalink",new BMessage(kCopyPermalink)));
    menu.AddItem(new BMenuItem("File History",new BMessage(kFileHistory)));
    menu.AddSeparatorItem();
    menu.AddItem(new BMenuItem("Find…",new BMessage(kFind)));
    menu.AddItem(new BMenuItem("Document Settings…",new BMessage(kDocumentSettings)));
    menu.AddItem(new BMenuItem("Go to Line…",new BMessage(kGoToLine)));
    menu.SetTargetForItems(Window());menu.Go(where,true,true);
}

void Editor::ClearSearchHighlights() {
    SendMessage(SCI_SETINDICATORCURRENT,8);SendMessage(SCI_INDICATORCLEARRANGE,0,SendMessage(SCI_GETLENGTH));
}
void Editor::SetSearchSelection(bool enabled) {
    fSelectionSearch=enabled;fLastEmpty=-1;
    fScopeStart=SendMessage(SCI_GETSELECTIONSTART);fScopeEnd=SendMessage(SCI_GETSELECTIONEND);
    fScopeText=enabled?Text():std::string();
}
TextMatches Editor::SearchMatches(const SearchOptions& options,bool highlight) {
    if(highlight) ClearSearchHighlights();fSearchError.clear();
    TextMatches result;
    if(fSelectionSearch && (!Matches(fScopeText) || fScopeStart==fScopeEnd)) result.error="Select text and enable In Selection again to capture a search scope.";
    else { auto text=Text();result=TextQuery(options).Find(text,fSelectionSearch?fScopeStart:0,fSelectionSearch?fScopeEnd:SIZE_MAX); }
    fSearchError=result.error;
    if(highlight && result.error.empty()) {
        SendMessage(SCI_INDICSETSTYLE,8,INDIC_ROUNDBOX);
        SendMessage(SCI_INDICSETFORE,8,fTheme.accent.red|(fTheme.accent.green<<8)|(fTheme.accent.blue<<16));
        SendMessage(SCI_INDICSETALPHA,8,65);SendMessage(SCI_INDICSETUNDER,8,1);
        for(const auto& match:result.matches) if(match.end>match.start) SendMessage(SCI_INDICATORFILLRANGE,match.start,match.end-match.start);
    }
    return result;
}
bool Editor::Find(const SearchOptions& options,bool backwards) {
    auto found=SearchMatches(options);if(found.matches.empty()) return false;
    auto key=options.query+char(options.regex)+char(options.matchCase)+char(options.wholeWord);
    if(key!=fLastQuery) { fLastQuery=key;fLastEmpty=-1; }
    auto start=SendMessage(SCI_GETSELECTIONSTART),end=SendMessage(SCI_GETSELECTIONEND);
    const TextEdit* selected=nullptr;
    if(backwards) {
        for(auto it=found.matches.rbegin();it!=found.matches.rend();++it)
            if(it->end<=size_t(start) && !(it->start==it->end && sptr_t(it->start)==fLastEmpty)) { selected=&*it;break; }
        if(!selected) selected=&found.matches.back();
    } else {
        for(const auto& match:found.matches)
            if(match.start>=size_t(end) && !(match.start==match.end && sptr_t(match.start)==fLastEmpty)) { selected=&match;break; }
        if(!selected) selected=&found.matches.front();
    }
    fLastEmpty=selected->start==selected->end?sptr_t(selected->start):-1;
    SendMessage(SCI_SETSEL,selected->start,selected->end);SendMessage(SCI_SCROLLCARET);return true;
}
int Editor::Replace(const SearchOptions& options,const std::string& replacement,bool all) {
    fSearchError.clear();
    if(SendMessage(SCI_GETREADONLY)) { fSearchError="This document is read-only.";return 0; }
    auto initial=SearchMatches(options,false);if(!initial.error.empty()) return 0;
    auto text=Text();auto found=TextQuery(options).Find(text,fSelectionSearch?fScopeStart:0,fSelectionSearch?fScopeEnd:SIZE_MAX,100000,nullptr,&replacement);
    if(!found.error.empty() || found.truncated) { fSearchError=found.truncated?"More than 100,000 matches. Narrow the selection before replacing.":found.error;return 0; }
    if(found.matches.empty()) return 0;
    std::vector<TextEdit> edits;
    if(all) edits=std::move(found.matches);
    else {
        auto start=SendMessage(SCI_GETSELECTIONSTART),end=SendMessage(SCI_GETSELECTIONEND);
        auto chosen=found.matches.end();
        for(auto it=found.matches.begin();it!=found.matches.end();++it) if(it->start==size_t(start) && it->end==size_t(end)) { chosen=it;break; }
        if(chosen==found.matches.end()) chosen=std::find_if(found.matches.begin(),found.matches.end(),[&](const auto& m){return m.start>=size_t(end);});
        if(chosen==found.matches.end()) chosen=found.matches.begin();
        edits.push_back(*chosen);
    }
    ApplyEdits(edits);
    if(fSelectionSearch) {
        for(const auto& edit:edits) fScopeEnd=fScopeEnd-(edit.end-edit.start)+edit.text.size();
        fScopeText=Text();
    }
    if(!all) {
        auto position=edits[0].start+edits[0].text.size();
        if(edits[0].start==edits[0].end) position=SendMessage(SCI_POSITIONAFTER,position);
        SendMessage(SCI_SETSEL,position,position);
    }
    fLastEmpty=-1;SearchMatches(options);return int(edits.size());
}
bool Editor::Find(const std::string& query,bool backwards,bool matchCase,bool regex) { return Find(SearchOptions{query,matchCase,regex,false},backwards); }
bool Editor::ReplaceOne(const std::string& query,const std::string& replacement,bool matchCase) { return Replace(SearchOptions{query,matchCase,false,false},replacement,false)>0; }
int Editor::ReplaceAll(const std::string& query,const std::string& replacement,bool matchCase) { return Replace(SearchOptions{query,matchCase,false,false},replacement,true); }
void Editor::GoTo(size_t line,size_t column,bool focus) {
    sptr_t pos=SendMessage(SCI_POSITIONFROMLINE,line?line-1:0);
    if(pos<0) pos=SendMessage(SCI_GETLENGTH);
    auto end=SendMessage(SCI_GETLINEENDPOSITION,SendMessage(SCI_LINEFROMPOSITION,pos));
    for(size_t i=1;i<column && pos<end;++i) pos=SendMessage(SCI_POSITIONAFTER,pos);
    SendMessage(SCI_GOTOPOS,pos);SendMessage(SCI_SCROLLCARET);if(focus) MakeFocus();
}
}
