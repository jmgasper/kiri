#pragma once
#include <ScintillaView.h>
#include "ui/Theme.h"
#include "ui/EditorSettings.h"
#include "core/LanguageProtocol.h"
#include <string>
#include <ILoader.h>
#include <memory>
namespace kiri {
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
    std::string Text();
    bool Matches(std::string_view text);
    void ApplyEdits(const std::vector<TextEdit>& edits,bool preserveLines=false);
    void ShowCompletions(const std::vector<std::string>& labels);
    void CancelCompletions();
    bool FilterLanguageKey(BMessage* message);
    bool Dirty() { return fRecovered || SendMessage(SCI_GETMODIFY)!=0; }
    void MarkRecovered() { fRecovered=true;++fRevision; }
    void MarkSaved() { fRecovered=false;SendMessage(SCI_SETSAVEPOINT); }
    int64 Revision() const { return fRevision; }
    const std::string& Language() const { return fLanguage; }
    bool Find(const std::string& query,bool backwards=false,bool matchCase=false,bool regex=false);
    bool ReplaceOne(const std::string& query,const std::string& replacement,bool matchCase=false);
    int ReplaceAll(const std::string& query,const std::string& replacement,bool matchCase=false);
    void GoTo(size_t line,size_t column=1);
private:
    void Style(int id,rgb_color color,bool bold=false);
    void UpdateMarginWidth();
    std::string fLexer="null",fLanguage="Plain Text";
    Theme fTheme=Theme::Builtins()[0];
    EditorSettings fSettings;
    bool fLoading=false,fRecovered=false;
    int fDigits=0;
    int64 fRevision=0;
    std::vector<std::string> fCompletionLabels;
};
}
