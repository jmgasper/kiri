#include "ui/LanguageToolsWindow.h"
#include "ui/Theme.h"
#include "ui/Messages.h"
#include <Alert.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TextControl.h>

namespace kiri {
namespace { enum:uint32 { kSelectLanguage='lgsl',kApply='lgok',kDefaults='lgdf' }; }
LanguageToolsWindow::LanguageToolsWindow(BMessenger target,LanguageTools tools,std::string settings,BRect parent)
    :BWindow(BRect(0,0,639,359),"Language Tools — Kiri",B_TITLED_WINDOW,B_NOT_ZOOMABLE|B_NOT_RESIZABLE|B_AUTO_UPDATE_SIZE_LIMITS|B_ASYNCHRONOUS_CONTROLS|B_CLOSE_ON_ESCAPE),
        fTarget(target),fTools(std::move(tools)),fSettings(std::move(settings)) {
    fPrettier=new BTextControl("prettier command","Prettier command",fTools.prettier.c_str(),nullptr);
    fCommand=new BTextControl("server command","Server command","",nullptr);
    fAutomatic=new BCheckBox("automatic completion","Suggest completions while typing",nullptr);fAutomatic->SetValue(fTools.completion?B_CONTROL_ON:B_CONTROL_OFF);
    fLanguages=new BPopUpMenu("Language");
    for(size_t i=0;i<fTools.profiles.size();++i) { auto* request=new BMessage(kSelectLanguage);request->AddInt32("language",i);fLanguages->AddItem(new BMenuItem(fTools.profiles[i].name.c_str(),request)); }
    auto* language=new BMenuField("language server","",fLanguages);fHint=new BStringView("language extensions","");
    auto* apply=new BButton("apply language tools","Apply",new BMessage(kApply));auto* done=new BButton("language tools done","Done",new BMessage(kApply));done->Message()->AddBool("close",true);
    auto* panel=new BView("language tools panel",B_WILL_DRAW);
    BLayoutBuilder::Group<>(panel,B_VERTICAL,12).SetInsets(20)
        .Add(new BStringView("format label","Formatting")).Add(fPrettier)
        .Add(new BStringView("format help","Project-local Prettier and project formatting settings are used automatically."))
        .AddStrut(10).Add(new BStringView("servers label","Language servers"))
        .AddGroup(B_HORIZONTAL,8).Add(new BStringView("language label","Language")).Add(language).End().Add(fCommand).Add(fHint).Add(fAutomatic)
        .Add(new BStringView("server help","Use a command with arguments; quote paths with spaces. Leave empty to disable."))
        .AddGroup(B_HORIZONTAL,8).Add(new BButton("language defaults","Restore Defaults",new BMessage(kDefaults)))
            .AddGlue().Add(new BButton("language cancel","Cancel",new BMessage(B_QUIT_REQUESTED))).Add(apply).Add(done).End();
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(panel);
    SetDefaultButton(done);Select(0);MoveTo(parent.left+(parent.Width()-Bounds().Width())/2,parent.top+(parent.Height()-Bounds().Height())/2);
}
void LanguageToolsWindow::Select(int index) {
    if(index<0 || index>=static_cast<int>(fTools.profiles.size())) { fCommand->SetEnabled(false);return; }
    fSelected=index;fCommand->SetText(fTools.profiles[index].command.c_str());fLanguages->ItemAt(index)->SetMarked(true);
    std::string extensions="Files: ";for(const auto& value:fTools.profiles[index].extensions) { if(extensions!="Files: ") extensions+=", ";extensions+=value; }fHint->SetText(extensions.c_str());
}
bool LanguageToolsWindow::Store() {
    if(fSelected>=0 && fSelected<static_cast<int>(fTools.profiles.size())) fTools.profiles[fSelected].command=fCommand->Text();
    fTools.prettier=fPrettier->Text();fTools.completion=fAutomatic->Value()==B_CONTROL_ON;
    auto error=fTools.Save(fSettings);if(error.empty()) { fTarget.SendMessage(kApplyLanguageTools);return true; }
    (new BAlert("Language Tools",error.c_str(),"OK"))->Go();return false;
}
void LanguageToolsWindow::MessageReceived(BMessage* message) {
    switch(message->what) {
        case kWindowTheme:ThemeWindow(this,*message);break;
        case kShowLanguageTools:Activate();break;
        case kSelectLanguage: {
            if(fSelected>=0 && fSelected<static_cast<int>(fTools.profiles.size())) fTools.profiles[fSelected].command=fCommand->Text();
            int32 index=0;if(message->FindInt32("language",&index)==B_OK) Select(index);break;
        }
        case kApply: { bool close=false;if(Store() && message->FindBool("close",&close)==B_OK && close) PostMessage(B_QUIT_REQUESTED);break; }
        case kDefaults: {
            LanguageTools defaults;fTools.prettier=defaults.prettier;fTools.completion=defaults.completion;
            for(auto& profile:fTools.profiles) for(const auto& original:defaults.profiles) if(profile.language==original.language) {
                profile.command=original.command;profile.initializationOptions=original.initializationOptions;profile.configuration=original.configuration;
            }
            fPrettier->SetText(fTools.prettier.c_str());fAutomatic->SetValue(B_CONTROL_ON);Select(fSelected);break;
        }
        default:BWindow::MessageReceived(message);
    }
}
}
