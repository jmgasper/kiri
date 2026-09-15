#pragma once
#include <ScintillaView.h>
#include "ui/Theme.h"
#include "ui/EditorSettings.h"
#include "core/LanguageProtocol.h"
#include "core/Search.h"
#include "core/LanguageAnalysis.h"
#include <string>
#include <ILoader.h>
#include <memory>
namespace kiri {
// Scintilla owns the text and undo stack through each attached view's document
// reference. This state follows that same buffer, including synchronous input
// revisions (Haiku delivers Scintilla's notifications asynchronously).
struct EditorState {
    int64 revision=0,inputRevision=0,changes=0;bool recovered=false;
    std::vector<Diagnostic> diagnostics;
    std::vector<SemanticToken> semanticTokens;
};
struct EditorLoader {
    Scintilla::ILoader* loader=nullptr;
    ~EditorLoader() { if(loader) loader->Release(); }
    void* Take() { auto* document=loader->ConvertToDocument();loader=nullptr;return document; }
};
class Editor : public BScintillaView {
public:
    Editor();
    void AllAttached() override;
    sptr_t SendMessage(unsigned int message,uptr_t wParam=0,sptr_t lParam=0);
    void NotificationReceived(SCNotification* notification) override;
    void ContextMenu(BPoint where) override;
    void ApplyTheme(const Theme& theme);
    void ApplySettings(const EditorSettings& settings);
    void SetLanguage(const std::string& path,bool large=false);
    void SetText(const std::string& bytes,bool readOnly=false,int eol=2);
    std::shared_ptr<EditorLoader> CreateLoader(bool large);
    void Adopt(EditorLoader& loader,int eol=2);
    void ShareDocument(Editor& source);
    std::shared_ptr<EditorState> State() const { return fState; }
    void NoteInput();
    void SetDiagnostics(const std::vector<Diagnostic>& diagnostics);
    void SetSemanticTokens(const std::vector<SemanticToken>& tokens);
    void ClearAnalysis();
    void ShowDiagnostic(size_t position);
    int64 InputRevision() const { return fState->inputRevision; }
    std::string Text();
    bool Matches(std::string_view text);
    void ApplyEdits(const std::vector<TextEdit>& edits,bool preserveLines=false);
    void ShowCompletions(const std::vector<std::string>& labels);
    void CancelCompletions();
    bool FilterLanguageKey(BMessage* message);
    bool Dirty() { return fState->recovered || SendMessage(SCI_GETMODIFY)!=0; }
    void MarkRecovered() { fState->recovered=true;++fState->changes;NoteInput(); }
    void MarkSaved() { fState->recovered=false;SendMessage(SCI_SETSAVEPOINT); }
    int64 Revision() const { return fState->revision; }
    const std::string& Language() const { return fLanguage; }
    bool Find(const std::string& query,bool backwards=false,bool matchCase=false,bool regex=false);
    bool ReplaceOne(const std::string& query,const std::string& replacement,bool matchCase=false);
    int ReplaceAll(const std::string& query,const std::string& replacement,bool matchCase=false);
    void SetSearchSelection(bool enabled);
    TextMatches SearchMatches(const SearchOptions& options,bool highlight=true);
    bool Find(const SearchOptions& options,bool backwards=false);
    int Replace(const SearchOptions& options,const std::string& replacement,bool all);
    const std::string& SearchError() const { return fSearchError; }
    void ClearSearchHighlights();
    void GoTo(size_t line,size_t column=1,bool focus=true);
private:
    void Style(int id,rgb_color color,bool bold=false);
    void UpdateMarginWidth();
    std::string fLexer="null",fLanguage="Plain Text";
    Theme fTheme=Theme::Builtins()[0];
    EditorSettings fSettings;
    bool fLoading=false;
    int fDigits=0;
    std::shared_ptr<EditorState> fState=std::make_shared<EditorState>();
    std::vector<std::string> fCompletionLabels;
    bool fSelectionSearch=false;
    size_t fScopeStart=0,fScopeEnd=0;
    std::string fScopeText,fSearchError,fLastQuery;
    sptr_t fLastEmpty=-1;
};
}
