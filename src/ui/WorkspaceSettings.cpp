#include "ui/Workspace.h"
#include "ui/DocumentSettingsWindow.h"
#include "ui/Editor.h"
#include "ui/Messages.h"
#include <StringView.h>

namespace kiri {
void Workspace::ShowDocumentSettings() {
    auto* d=Current();if(!d || !d->editor || d->editor->SendMessage(SCI_GETREADONLY)) {Notice("Open an editable text document first.");return;}
    if(d->settingsWindow.IsValid()) {d->settingsWindow.SendMessage(kActivateWorkspace);return;}
    auto* window=new DocumentSettingsWindow(BMessenger(this),fEditorSettings,d->editor->EffectiveStyle(),d->editor->State()->overrides,Frame(),d->id,d->name);
    d->settingsWindow=BMessenger(window);window->Show();
}
void Workspace::UpdateDocumentStyle(Document& d) {
    for(auto& pane:fPanes) for(auto& tab:pane->tabs) if(tab->document==d.id && tab->editor) tab->editor->ApplyDocumentStyle();
    UpdateStatus();
}
void Workspace::ApplyDocumentSettings(BMessage& message) {
    int64 id=0;if(message.FindInt64("document",&id)!=B_OK) return;auto* d=ByID(id);if(!d || !d->editor) return;
    d->editor->State()->overrides=ReadDocumentOverrides(message);UpdateDocumentStyle(*d);SaveSettings();
}
void Workspace::RefreshDocumentConfig(Document& d,bool force) {
    if(!d.editor || d.path.empty() || d.editor->SendMessage(SCI_GETREADONLY) || (d.configPending && !force)) return;
    auto id=d.id,serial=++d.configSerial;auto path=d.path;auto stamps=d.editor->State()->config.stamps;d.configPending=true;
    fJobs->Submit([this,id,serial,path,stamps=std::move(stamps),force](const auto& cancel) {
        bool changed=force || stamps.empty();for(auto& entry:stamps) {if(cancel) return AsyncQueue::Callback{};if(StatFile(entry.first)!=entry.second) changed=true;}
        std::shared_ptr<DocumentConfig> config;if(changed && !cancel) config=std::make_shared<DocumentConfig>(ReadDocumentConfig(path,&cancel));
        return AsyncQueue::Callback([this,id,serial,path,config] {auto* d=ByID(id);if(!d || d->path!=path || d->configSerial!=serial) return;d->configPending=false;
            if(config && d->editor) {d->editor->State()->config=*config;UpdateDocumentStyle(*d);}});
    },"document-config-"+std::to_string(id),[this,id,serial](const std::string& error){if(auto* d=ByID(id);d && d->configSerial==serial) {d->configPending=false;Notice("EditorConfig: "+error);}});
}
}
