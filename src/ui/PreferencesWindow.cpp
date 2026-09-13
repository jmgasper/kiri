#include "ui/PreferencesWindow.h"
#include "ui/Editor.h"
#include "ui/Messages.h"
#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <StringView.h>
#include <TextControl.h>
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace kiri {
namespace {
enum:uint32 { kFont='pfnt',kSize='pszs',kThemeChoice='pthm',kApply='papl',kOK='pacc',kDefaults='pdfs' };
}
PreferencesWindow::PreferencesWindow(BMessenger target,const EditorSettings& settings,BRect parentFrame)
    :BWindow(BRect(0,0,610,485),"Editor Preferences — Kiri",B_TITLED_WINDOW,
        B_NOT_ZOOMABLE|B_NOT_RESIZABLE|B_AUTO_UPDATE_SIZE_LIMITS|B_ASYNCHRONOUS_CONTROLS|B_CLOSE_ON_ESCAPE),
      fTarget(target),fApplied(settings),fPending(settings) {
    auto* title=new BStringView("preferences title","Editor preferences");
    BFont heading(be_bold_font);heading.SetSize(18);title->SetFont(&heading);
    auto* description=new BStringView("preferences description","Choose the appearance of source files and Git diffs.");

    auto* fonts=new BPopUpMenu("font families",true,true);
    std::vector<std::string> families;
    for(int32 i=0;i<count_font_families();++i) {
        font_family family;
        if(get_font_family(i,&family)==B_OK) families.emplace_back(family);
    }
    std::sort(families.begin(),families.end());
    for(const auto& family:families) {
        auto* message=new BMessage(kFont);message->AddString("editor_font_family",family.c_str());
        fonts->AddItem(new BMenuItem(family.c_str(),message));
    }
    fFont=new BMenuField("editor font","Font:",fonts);
    fSize=new BTextControl("editor font size","Size (pt):","",new BMessage(kSize));
    fSize->SetModificationMessage(new BMessage(kSize));
    fSize->TextView()->SetMaxBytes(2);
    fTheme=new BMenuField("editor theme","Theme:",ThemeMenu("themes",kThemeChoice,settings.theme));
    fValidation=new BStringView("preferences validation"," ");

    fPreview=new Editor();fPreview->SetName("preferences preview");
    fPreview->SetLanguage("preview.cpp");
    fPreview->SetText("// Your editor, your style.\n#include <string>\n\nint main() {\n    const std::string message = \"Hello, Kiri!\";\n    return message.empty() ? 1 : 0;\n}\n",true);
    fPreview->SetExplicitMinSize(BSize(550,190));
    auto* defaults=new BButton("preferences defaults","Restore Defaults",new BMessage(kDefaults));
    auto* cancel=new BButton("preferences cancel","Cancel",new BMessage(B_QUIT_REQUESTED));
    fApply=new BButton("preferences apply","Apply",new BMessage(kApply));
    fOK=new BButton("preferences ok","OK",new BMessage(kOK));
    BLayoutBuilder::Group<>(this,B_VERTICAL,10).SetInsets(18)
        .Add(title).Add(description)
        .AddGrid(12,10)
            .Add(fFont->CreateLabelLayoutItem(),0,0).Add(fFont->CreateMenuBarLayoutItem(),1,0)
            .Add(fSize->CreateLabelLayoutItem(),0,1).Add(fSize->CreateTextViewLayoutItem(),1,1)
            .Add(fTheme->CreateLabelLayoutItem(),0,2).Add(fTheme->CreateMenuBarLayoutItem(),1,2)
        .End()
        .Add(fValidation)
        .Add(new BStringView("preview title","Preview"))
        .Add(fPreview)
        .AddGroup(B_HORIZONTAL,8).Add(defaults).AddGlue().Add(cancel).Add(fApply).Add(fOK).End();
    fonts->SetTargetForItems(this);fTheme->Menu()->SetTargetForItems(this);
    SetDefaultButton(fOK);LoadControls();
    BRect screen=BScreen(this).Frame();
    MoveTo(std::clamp(parentFrame.left+(parentFrame.Width()-Bounds().Width())/2,screen.left,screen.right-Bounds().Width()),
        std::clamp(parentFrame.top+45,screen.top+30,screen.bottom-Bounds().Height()));
}
void PreferencesWindow::LoadControls() {
    if(auto* item=fFont->Menu()->FindItem(fPending.fontFamily.c_str())) item->SetMarked(true);
    fSize->SetText(std::to_string(fPending.fontSize).c_str());
    MarkTheme(fTheme->Menu(),fPending.theme);UpdatePreview();
}
bool PreferencesWindow::UpdatePreview() {
    char* end=nullptr;long size=std::strtol(fSize->Text(),&end,10);
    bool valid=*fSize->Text() && end && !*end && size>=8 && size<=48;
    fSize->MarkAsInvalid(!valid);
    fValidation->SetText(valid?"Changes apply to open files and new documents.":"Enter a font size from 8 to 48 points.");
    fOK->SetEnabled(valid);
    if(!valid) { fApply->SetEnabled(false);return false; }
    fPending.fontSize=size;fPreview->ApplySettings(fPending);
    fApply->SetEnabled(fPending!=fApplied);return true;
}
bool PreferencesWindow::Apply() {
    if(!UpdatePreview()) return false;
    BMessage message(kApplyPreferences);fPending.WriteTo(message);
    if(fTarget.SendMessage(&message)!=B_OK) return false;
    fApplied=fPending;fApply->SetEnabled(false);return true;
}
void PreferencesWindow::MessageReceived(BMessage* message) {
    switch(message->what) {
        case kFont: {
            const char* family;
            if(message->FindString("editor_font_family",&family)==B_OK) fPending.fontFamily=family;
            UpdatePreview();break;
        }
        case kSize:UpdatePreview();break;
        case kThemeChoice: {
            const char* theme;
            if(message->FindString("theme_name",&theme)==B_OK) fPending.theme=ThemeIndex(theme);
            UpdatePreview();break;
        }
        case kDefaults:fPending=EditorSettings();LoadControls();break;
        case kApply:Apply();break;
        case kOK:if(Apply()) PostMessage(B_QUIT_REQUESTED);break;
        case kShowPreferences:Activate();break;
        case kSyncPreferences:fApplied.ReadFrom(*message);fPending=fApplied;LoadControls();break;
        default:BWindow::MessageReceived(message);break;
    }
}
}
