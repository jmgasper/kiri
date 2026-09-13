#include "ui/LauncherWindow.h"
#include "ui/EditorSettings.h"
#include "ui/FileIcons.h"
#include "ui/Messages.h"
#include "ui/Theme.h"
#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <CardLayout.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Screen.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <String.h>
#include <algorithm>
#include <filesystem>

namespace kiri {
namespace {
enum:uint32 { kRecentSelection='rcsl',kOpenRecent='rcop' };
class ActionButton:public BButton {
public:
    ActionButton(const char* name,const char* label,uint32 command,bool icon=false,bool folder=false)
        :BButton(name,label,new BMessage(command)),fIcon(icon?FileIcon("file.txt",folder):nullptr) {
        SetExplicitAlignment(BAlignment(B_ALIGN_USE_FULL_WIDTH,B_ALIGN_USE_FULL_HEIGHT));
        SetExplicitMinSize(BSize(icon?175:74,36));SetExplicitMaxSize(BSize(icon?B_SIZE_UNLIMITED:74,36));
    }
    void ApplyTheme(const Theme& theme) { fTheme=theme;Invalidate(); }
    void Draw(BRect) override {
        auto frame=Bounds();SetHighColor(Value()==B_CONTROL_ON?fTheme.selection:fTheme.toolbar);FillRoundRect(frame,3,3);
        SetHighColor(IsEnabled() && (IsFocus() || IsDefault())?fTheme.accent:fTheme.border);StrokeRoundRect(frame,3,3);
        float left=(frame.Width()-StringWidth(Label()))/2;
        if(fIcon) {
            PushState();SetDrawingMode(B_OP_ALPHA);SetBlendingMode(B_PIXEL_ALPHA,B_ALPHA_OVERLAY);
            DrawBitmap(fIcon.get(),BPoint(12,(frame.Height()-16)/2));PopState();left=39;
        }
        SetLowColor(Value()==B_CONTROL_ON?fTheme.selection:fTheme.toolbar);SetHighColor(IsEnabled()?fTheme.text:fTheme.muted);
        font_height height;GetFontHeight(&height);DrawString(Label(),BPoint(left,(frame.Height()+height.ascent-height.descent)/2));
    }
private:
    Theme fTheme;std::shared_ptr<const BBitmap> fIcon;
};
class RecentRow:public BStringItem {
public:
    RecentRow(const RecentItem& item,const Theme& theme):BStringItem(item.path.c_str()),fItem(item),fTheme(theme),fIcon(FileIcon(item.path,item.folder)) {}
    void Update(BView* owner,const BFont* font) override { BStringItem::Update(owner,font);SetHeight(std::max(54.f,font->Size()*3.6f)); }
    void DrawItem(BView* owner,BRect frame,bool) override {
        owner->SetHighColor(IsSelected()?fTheme.selection:fTheme.background);owner->FillRect(frame);owner->SetLowColor(owner->HighColor());
        auto path=std::filesystem::path(fItem.path);auto name=path.filename().string();if(name.empty()) name=fItem.path;
        if(fIcon) {
            owner->PushState();owner->SetDrawingMode(B_OP_ALPHA);owner->SetBlendingMode(B_PIXEL_ALPHA,B_ALPHA_OVERLAY);
            owner->DrawBitmap(fIcon.get(),BPoint(frame.left+12,frame.top+(Height()-16)/2));owner->PopState();
        }
        owner->SetFont(be_bold_font);owner->SetHighColor(fTheme.text);
        BString label(name.c_str());owner->TruncateString(&label,B_TRUNCATE_MIDDLE,frame.Width()-55);
        owner->DrawString(label.String(),BPoint(frame.left+40,frame.top+Height()/2-3));
        owner->SetFont(be_plain_font);owner->SetHighColor(fTheme.muted);
        BString detail((std::string(fItem.folder?"Folder  ·  ":"File  ·  ")+path.parent_path().string()).c_str());
        owner->TruncateString(&detail,B_TRUNCATE_MIDDLE,frame.Width()-55);
        owner->DrawString(detail.String(),BPoint(frame.left+40,frame.top+Height()/2+15));
    }
private:
    RecentItem fItem;Theme fTheme;std::shared_ptr<const BBitmap> fIcon;
};
class RecentList:public BListView {
public:
    RecentList():BListView("recent items",B_SINGLE_SELECTION_LIST) {}
    ~RecentList() override { while(auto* item=RemoveItem(int32(0))) delete item; }
};
}
LauncherWindow::LauncherWindow(const std::string& directory)
    :BWindow(BRect(0,0,759,499),"Welcome to Kiri",B_TITLED_WINDOW,
        B_NOT_ZOOMABLE|B_AUTO_UPDATE_SIZE_LIMITS|B_ASYNCHRONOUS_CONTROLS|B_CLOSE_ON_ESCAPE),fSettings(directory) {
    auto* title=new BStringView("launcher title","Kiri");BFont large(be_bold_font);large.SetSize(32);title->SetFont(&large);
    auto* subtitle=new BStringView("launcher subtitle","Open a folder or file to get started.");
    auto* start=new BStringView("start title","Start");start->SetFont(be_bold_font);
    auto* recent=new BStringView("recent title","Recent");recent->SetFont(be_bold_font);
    auto* folder=new ActionButton("launcher open folder","Open Folder…",kOpenProject,true,true);
    auto* file=new ActionButton("launcher open file","Open File…",kOpenFile,true);
    auto* create=new ActionButton("launcher new file","New File",kNewFile,true);
    fRecents=new RecentList();fRecents->SetInvocationMessage(new BMessage(kOpenRecent));fRecents->SetSelectionMessage(new BMessage(kRecentSelection));
    auto* recentHost=new BView("recent host",0);fRecentLayout=new BCardLayout();recentHost->SetLayout(fRecentLayout);
    auto* empty=new BView("no recent items",0);
    BLayoutBuilder::Group<>(empty,B_VERTICAL,8).SetInsets(18).AddGlue()
        .Add(new BStringView("recent empty title","No recent folders or files yet."))
        .Add(new BStringView("recent empty detail","Items you open will appear here.")).AddGlue();
    fRecentLayout->AddView(empty);
    fRecentLayout->AddView(new BScrollView("recent scroll",fRecents,0,false,true,B_NO_BORDER));
    recentHost->SetExplicitMinSize(BSize(380,260));
    fOpen=new ActionButton("open recent","Open",kOpenRecent);fOpen->SetEnabled(false);
    fHint=new BStringView("recent hint","Double-click an item, or select it and press Enter.");
    auto* body=new BView("launcher body",0);
    BLayoutBuilder::Group<>(body,B_VERTICAL,8).SetInsets(28)
        .Add(title).Add(subtitle).AddStrut(18)
        .AddGroup(B_HORIZONTAL,30)
            .AddGroup(B_VERTICAL,10,0).Add(start).Add(folder).Add(file).Add(create).AddGlue().End()
            .AddGroup(B_VERTICAL,10).Add(recent).Add(recentHost)
                .AddGroup(B_HORIZONTAL,10).Add(fHint).Add(fOpen).End()
            .End()
        .End();
    BLayoutBuilder::Group<>(this,B_VERTICAL,0).Add(body);
    AddShortcut('O',B_COMMAND_KEY|B_SHIFT_KEY,new BMessage(kOpenProject));
    AddShortcut('O',B_COMMAND_KEY,new BMessage(kOpenFile));
    AddShortcut('N',B_COMMAND_KEY,new BMessage(kNewFile));
    AddShortcut('Q',B_COMMAND_KEY,new BMessage(B_QUIT_REQUESTED));
    SetDefaultButton(fOpen);Refresh();
    BRect screen=BScreen(this).Frame();MoveTo((screen.Width()-Bounds().Width())/2,(screen.Height()-Bounds().Height())/2);
}
LauncherWindow::~LauncherWindow() = default;
void LauncherWindow::Refresh() {
    std::string selected;auto index=fRecents->CurrentSelection();if(index>=0 && index<static_cast<int32>(fItems.size())) selected=fItems[index].path;
    while(auto* item=fRecents->RemoveItem(int32(0))) delete item;
    BFile file((fSettings+"/settings").c_str(),B_READ_ONLY);BMessage stored;stored.Unflatten(&file);
    EditorSettings settings;settings.ReadFrom(stored);const auto& theme=Theme::Builtins()[settings.theme];
    for(int32 i=0;i<CountChildren();++i) ThemeView(ChildAt(i),theme);
    for(const char* name:{"launcher open folder","launcher open file","launcher new file","open recent"})
        static_cast<ActionButton*>(FindView(name))->ApplyTheme(theme);
    FindView("launcher subtitle")->SetHighColor(theme.muted);fHint->SetHighColor(theme.muted);
    fRecents->SetViewColor(theme.background);fRecents->SetLowColor(theme.background);
    fItems=RecentItems(fSettings).Load();
    for(size_t i=0;i<fItems.size();++i) { fRecents->AddItem(new RecentRow(fItems[i],theme));if(fItems[i].path==selected) fRecents->Select(i); }
    fRecentLayout->SetVisibleItem(fItems.empty()?int32(0):int32(1));
    fOpen->SetEnabled(fRecents->CurrentSelection()>=0);fHint->SetText(fItems.empty()?" ":"Double-click an item, or press Enter.");
}
void LauncherWindow::OpenRecent() {
    auto index=fRecents->CurrentSelection();if(index<0 || index>=static_cast<int32>(fItems.size())) return;
    auto item=fItems[index];std::error_code error;
    bool available=item.folder?std::filesystem::is_directory(item.path,error):std::filesystem::is_regular_file(item.path,error);
    if(!available) {
        auto text=std::string("This ")+(item.folder?"folder":"file")+" is no longer available:\n\n"+item.path;
        if((new BAlert("Recent Item",text.c_str(),"Cancel","Remove from Recent",nullptr,B_WIDTH_AS_USUAL,B_WARNING_ALERT))->Go()==1) {
            RecentItems(fSettings).Remove(item.path);Refresh();
        }
        return;
    }
    BMessage request(item.folder?kOpenProject:kOpenFile);request.AddString("path",item.path.c_str());be_app->PostMessage(&request);
}
void LauncherWindow::ShowPicker(bool folder) {
    auto& panel=folder?fFolderPanel:fFilePanel;
    if(!panel) {
        BMessenger target(this);BMessage chosen(folder?kProjectChosen:kFileChosen);
        panel=std::make_unique<BFilePanel>(B_OPEN_PANEL,&target,nullptr,folder?B_DIRECTORY_NODE:B_FILE_NODE,!folder,&chosen);
        panel->Window()->SetTitle(folder?"Open Folder — Kiri":"Open File — Kiri");
    }
    panel->Show();panel->Window()->Activate();
}
bool LauncherWindow::QuitRequested() { be_app->PostMessage(kLauncherClosed);return true; }
void LauncherWindow::MessageReceived(BMessage* message) {
    switch(message->what) {
        case kShowLauncher:Refresh();if(IsHidden()) Show();Activate();break;
        case kHideLauncher:if(fFolderPanel) fFolderPanel->Hide();if(fFilePanel) fFilePanel->Hide();if(!IsHidden()) Hide();break;
        case kRecentsChanged:if(!IsHidden()) Refresh();break;
        case kOpenProject:ShowPicker(true);break;
        case kOpenFile:ShowPicker(false);break;
        case kNewFile:be_app->PostMessage(kNewFile);break;
        case kFileChosen:case kProjectChosen: { BMessage refs(*message);refs.what=B_REFS_RECEIVED;be_app->PostMessage(&refs);break; }
        case kRecentSelection:fOpen->SetEnabled(fRecents->CurrentSelection()>=0);break;
        case kOpenRecent:OpenRecent();break;
        default:BWindow::MessageReceived(message);
    }
}
}
