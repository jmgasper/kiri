#include "ui/PreferencesWindow.h"
#include "ui/Editor.h"
#include "ui/Messages.h"
#include <Button.h>
#include <Font.h>
#include <CheckBox.h>
#include <FilePanel.h>
#include <Entry.h>
#include <Path.h>
#include "core/FileIO.h"
#include "ui/DocumentSettingsWindow.h"
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
enum:uint32 { kFont='pfnt',kSize='pszs',kThemeChoice='pthm',kApply='papl',kOK='pacc',kDefaults='pdfs',kDuplicate='thdu',kName='thnm',kColor='thcl',kRole='thrl',kImport='thim',kImported='thrd',kExport='thex',kExported='thwr',kSemantic='thsh' };
}
PreferencesWindow::PreferencesWindow(BMessenger target,const EditorSettings& settings,BRect parentFrame,const std::string& directory)
    :BWindow(BRect(0,0,690,660),"Editor Preferences — Kiri",B_TITLED_WINDOW,
        B_NOT_ZOOMABLE|B_NOT_RESIZABLE|B_AUTO_UPDATE_SIZE_LIMITS|B_ASYNCHRONOUS_CONTROLS|B_CLOSE_ON_ESCAPE),
      fTarget(target),fApplied(settings),fPending(settings),fDirectory(directory) {
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
    fFont=new BMenuField("editor font","",fonts);
    fSize=new BTextControl("editor font size","","",new BMessage(kSize));
    fSize->SetModificationMessage(new BMessage(kSize));
    fSize->TextView()->SetMaxBytes(2);
    fTheme=new BMenuField("editor theme","",ThemeMenu("themes",kThemeChoice,settings.theme,directory,settings.themeID));
    fValidation=new BStringView("preferences validation"," ");fValidation->SetTruncation(B_TRUNCATE_END);fValidation->SetExplicitMinSize(BSize(200,20));
    fName=new BTextControl("theme name","Name:","",nullptr);fName->SetModificationMessage(new BMessage(kName));
    fColor=new BTextControl("theme color","#RRGGBB:","",nullptr);fColor->SetModificationMessage(new BMessage(kColor));fColor->TextView()->SetMaxBytes(7);
    auto* roles=new BPopUpMenu("colors",true,true);for(const auto& role:ColorRoles()) {auto* message=new BMessage(kRole);message->AddString("role",role.c_str());roles->AddItem(new BMenuItem(role.c_str(),message));}
    fRole=new BMenuField("theme color role","",roles);roles->SetTargetForItems(this);
    fDark=new BCheckBox("theme dark","Dark theme",new BMessage(kColor));
    fSemantic=new BCheckBox("semantic highlighting","Language-server semantic highlighting",new BMessage(kSemantic));

    fPreview=new Editor();fPreview->SetName("preferences preview");
    fPreview->SetLanguage("preview.cpp");
    fPreview->SetText("// Your editor, your style.\nstruct Widget { int count; };\nint render(Widget value, int parameter) {\n    return value.count + parameter;\n}\n",true);
    fPreview->SetExplicitMinSize(BSize(600,180));
    auto* defaults=new BButton("preferences defaults","Restore Defaults",new BMessage(kDefaults));
    auto* cancel=new BButton("preferences cancel","Cancel",new BMessage(B_QUIT_REQUESTED));
    fApply=new BButton("preferences apply","Apply",new BMessage(kApply));
    fOK=new BButton("preferences ok","OK",new BMessage(kOK));
    auto* panel=new BView("preferences panel",B_WILL_DRAW);
    BLayoutBuilder::Group<>(panel,B_VERTICAL,10).SetInsets(18)
        .Add(title).Add(description)
        .AddGrid(12,10)
            .SetColumnWeight(0,0).SetColumnWeight(1,1)
            .Add(new BStringView("font label","Font:"),0,0).Add(fFont,1,0)
            .Add(new BStringView("size label","Size (pt):"),0,1).Add(fSize,1,1)
            .Add(new BStringView("theme label","Theme:"),0,2).Add(fTheme,1,2)
        .End()
        .AddGroup(B_HORIZONTAL,8)
            .Add(new BButton("duplicate theme","Duplicate Theme",new BMessage(kDuplicate)))
            .Add(new BButton("import theme","Import…",new BMessage(kImport)))
            .Add(new BButton("export theme","Export…",new BMessage(kExport))).AddGlue().End()
        .Add(fName)
        .AddGroup(B_HORIZONTAL,8).Add(new BStringView("color label","Color:")).Add(fRole).Add(fColor).Add(fDark).End()
        .Add(fSemantic).Add(fValidation)
        .Add(new BStringView("preview title","Preview · select a named color to edit it"))
        .Add(fPreview)
        .AddGroup(B_HORIZONTAL,8).Add(defaults).Add(new BButton("editing defaults","Editing Defaults…",new BMessage(kEditingDefaults))).AddGlue().Add(cancel).Add(fApply).Add(fOK).End();
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(panel);
    fonts->SetTargetForItems(this);fTheme->Menu()->SetTargetForItems(this);
    SetDefaultButton(fOK);LoadControls();
    BRect screen=BScreen(this).Frame();
    MoveTo(std::clamp(parentFrame.left+(parentFrame.Width()-Bounds().Width())/2,screen.left,screen.right-Bounds().Width()),
        std::clamp(parentFrame.top+45,screen.top+30,screen.bottom-Bounds().Height()));
}
PreferencesWindow::~PreferencesWindow() {if(fEditingWindow.IsValid()) fEditingWindow.SendMessage(B_QUIT_REQUESTED);}
void PreferencesWindow::ThemeChoices() {
    PopulateThemeMenu(fTheme->Menu(),kThemeChoice,fPending.theme,fDirectory,fPending.themeID);
    if(fPending.customTheme) {
        bool found=false;for(int32 i=0;i<fTheme->Menu()->CountItems();++i) { const char* id=nullptr;auto* item=fTheme->Menu()->ItemAt(i);if(item->Message() && item->Message()->FindString("theme_id",&id)==B_OK && fPending.themeID==id) {found=true;break;} }
        if(!found) {auto* message=new BMessage(kThemeChoice);message->AddString("theme_id",fPending.themeID.c_str());auto* item=new BMenuItem(fPending.customTheme->name.c_str(),message);fTheme->Menu()->AddItem(item);item->SetMarked(true);}
    }
    fTheme->Menu()->SetTargetForItems(this);
}
void PreferencesWindow::LoadControls() {
    fLoading=true;
    if(auto* item=fFont->Menu()->FindItem(fPending.fontFamily.c_str())) item->SetMarked(true);
    fSize->SetText(std::to_string(fPending.fontSize).c_str());ThemeChoices();
    auto theme=fPending.Colors();fName->SetText(theme.name.c_str());fColor->SetText(HexColor(theme.definition.colors.at(fColorRole)).c_str());
    fName->SetEnabled(bool(fPending.customTheme));fColor->SetEnabled(bool(fPending.customTheme));fDark->SetEnabled(bool(fPending.customTheme));fDark->SetValue(theme.dark);
    if(auto* role=fRole->Menu()->FindItem(fColorRole.c_str())) role->SetMarked(true);
    fSemantic->SetValue(fPending.semanticHighlighting);fLoading=false;UpdatePreview();
}
bool PreferencesWindow::UpdatePreview() {
    if(fLoading) return false;
    char* end=nullptr;long size=std::strtol(fSize->Text(),&end,10);bool valid=*fSize->Text() && end && !*end && size>=8 && size<=48;
    std::string error=valid?"":"Enter a font size from 8 to 48 points.";fSize->MarkAsInvalid(!valid);
    uint32_t color=0;bool colorValid=ParseColor(fColor->Text(),color);fColor->MarkAsInvalid(!colorValid);
    if(fPending.customTheme) {
        if(!colorValid) {valid=false;error="Use a color in #RRGGBB format.";}
        else {
            auto theme=*fPending.customTheme;theme.name=fName->Text();theme.dark=fDark->Value();theme.colors[fColorRole]=color;
            auto read=ParseTheme(SerializeTheme(theme));if(!read.ok()) {valid=false;error=read.error;}else fPending.SetTheme(theme);
        }
    }
    fOK->SetEnabled(valid);fApply->SetEnabled(valid && fPending!=fApplied);
    if(!valid) {fValidation->SetText(error.c_str());fValidation->SetToolTip(error.c_str());return false;}
    fPending.fontSize=size;fPending.semanticHighlighting=fSemantic->Value();fPreview->ApplySettings(fPending);
    auto sample=fPreview->Text();std::vector<SemanticToken> tokens;
    if(fPending.semanticHighlighting) for(auto pair:{std::pair<const char*,SemanticRole>{"Widget",SemanticRole::Type},{"parameter",SemanticRole::Parameter},{"count",SemanticRole::Property},{"render",SemanticRole::Function},{"value",SemanticRole::Variable}}) {
        for(size_t at=sample.find(pair.first);at!=std::string::npos;at=sample.find(pair.first,at+1)) tokens.push_back({at,at+std::string(pair.first).size(),pair.second,false});
    }
    fPreview->SetSemanticTokens(tokens);
    auto count=sample.find("count");fPreview->SetDiagnostics({{count,count+5,1,20,2,"Example warning for the preview","Kiri",""}});
    ThemeView(ChildAt(0),fPending.Colors());
    // Grid labels need a real minimum width: the generic theme pass permits
    // status labels to shrink, while this column has no stretch weight.
    for(const char* name:{"font label","size label","theme label","color label"}) {
        auto* label=static_cast<BStringView*>(FindView(name));BSize size(label->StringWidth(label->Text())+2,B_SIZE_UNSET);
        label->SetExplicitMinSize(size);label->SetExplicitMaxSize(size);
    }
    auto status=fThemeNotice.empty()?fPending.themeStatus:fThemeNotice;
    if(status.empty()) status=ThemeContrastWarning(fPending.Colors().definition);
    if(status.empty()) status=fPending.customTheme?"Apply saves this custom theme. Cancel keeps the applied appearance.":"Duplicate a built-in theme to edit its colors.";
    fValidation->SetText(status.c_str());fValidation->SetToolTip(status.c_str());fApply->SetEnabled(fPending!=fApplied);return true;
}
bool PreferencesWindow::Apply() {
    if(!UpdatePreview()) return false;
    if(fPending.customTheme) {
        auto error=SaveColorTheme(fDirectory,*fPending.customTheme);
        if(!error.empty()) {fThemeNotice=error;fValidation->SetText(error.c_str());return false;}
    }
    BMessage message(kApplyPreferences);fPending.WriteTo(message);
    if(fTarget.SendMessage(&message)!=B_OK) return false;
    fApplied=fPending;fApply->SetEnabled(false);return true;
}
void PreferencesWindow::ReadTheme(BMessage& message) {
    entry_ref ref;if(message.FindRef("refs",&ref)!=B_OK) return;BEntry entry(&ref,true);BPath path;if(entry.GetPath(&path)!=B_OK) return;
    auto result=ReadThemeFile(path.Path());if(!result.ok()) {fThemeNotice=result.error;fValidation->SetText(result.error.c_str());return;}
    auto imported=result.theme;bool duplicate=imported.id.rfind("custom.",0)!=0;
    for(const auto& saved:LoadColorThemes(fDirectory)) if(saved.id==imported.id && !(saved==imported)) duplicate=true;
    if(duplicate) imported.id=NewThemeID();fPending.SetTheme(imported);fThemeNotice=result.warning;LoadControls();
}
void PreferencesWindow::ExportTheme(BMessage& message) {
    entry_ref ref;const char* name=nullptr;if(message.FindRef("directory",&ref)!=B_OK || message.FindString("name",&name)!=B_OK) return;
    BEntry directory(&ref,true);BPath path;if(directory.GetPath(&path)!=B_OK || path.Append(name)!=B_OK) return;
    auto error=SaveFile(path.Path(),SerializeTheme(fPending.Colors().definition),StatFile(path.Path()));
    fThemeNotice=error.empty()?"Theme exported.":error;fValidation->SetText(fThemeNotice.c_str());fValidation->SetToolTip(fThemeNotice.c_str());
}
void PreferencesWindow::MessageReceived(BMessage* message) {
    switch(message->what) {
        case kEditingDefaults: {
            if(fEditingWindow.IsValid()) {fEditingWindow.SendMessage(kActivateWorkspace);break;}
            auto style=ResolveDocumentStyle(fPending.indentation,fPending.guideColumns,{});auto* window=new DocumentSettingsWindow(BMessenger(this),fPending,style,{},Frame());fEditingWindow=BMessenger(window);window->Show();break;
        }
        case kApplyEditingDefaults: {EditorSettings settings=fPending;settings.ReadFrom(*message);fPending.indentation=settings.indentation;fPending.guideColumns=settings.guideColumns;fPending.minimap=settings.minimap;UpdatePreview();break;}
        case kFont: {const char* family;if(message->FindString("editor_font_family",&family)==B_OK) fPending.fontFamily=family;UpdatePreview();break;}
        case kSize:case kName:case kColor:case kSemantic:UpdatePreview();break;
        case kThemeChoice: {
            const char* id=nullptr;if(message->FindString("theme_id",&id)==B_OK && fPending.themeID!=id) {fPending.SelectTheme(id,fDirectory);fThemeNotice.clear();LoadControls();}break;
        }
        case kRole: {const char* role=nullptr;if(message->FindString("role",&role)==B_OK && fPending.Colors().definition.colors.count(role)) {fColorRole=role;LoadControls();}break;}
        case kDuplicate: {auto theme=fPending.Colors().definition;theme.id=NewThemeID();theme.name+=" Custom";fPending.SetTheme(theme);fThemeNotice.clear();LoadControls();break;}
        case kImport:case kExport: {
            if(message->what==kExport && !UpdatePreview()) break;
            BMessenger target(this);BMessage reply(message->what==kImport?kImported:kExported);
            fThemePanel=std::make_unique<BFilePanel>(message->what==kImport?B_OPEN_PANEL:B_SAVE_PANEL,&target,nullptr,B_FILE_NODE,false,&reply);
            if(message->what==kExport) {auto name=fPending.Colors().name+".kiri-theme.json";std::replace(name.begin(),name.end(),'/','_');fThemePanel->SetSaveText(name.c_str());}
            fThemePanel->Show();break;
        }
        case kImported:ReadTheme(*message);break;
        case kExported:ExportTheme(*message);break;
        case kDefaults:fPending=EditorSettings();fThemeNotice.clear();LoadControls();break;
        case kApply:Apply();break;
        case kOK:if(Apply()) PostMessage(B_QUIT_REQUESTED);break;
        case kShowPreferences:Activate();break;
        case kSyncPreferences:fApplied.ReadFrom(*message);fPending=fApplied;fThemeNotice.clear();LoadControls();break;
        default:BWindow::MessageReceived(message);break;
    }
}
}
