#include "ui/SymbolBar.h"
#include "ui/Messages.h"
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <String.h>
#include <Window.h>

namespace kiri {
SymbolBar::SymbolBar():BView("file symbols",B_WILL_DRAW|B_NAVIGABLE|B_FRAME_EVENTS) {
    SetExplicitMinSize(BSize(160,29));SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED,29));SetViewColor(B_TRANSPARENT_COLOR);
}
void SymbolBar::ApplyTheme(const Theme& theme) { fTheme=theme;Invalidate(); }
void SymbolBar::SetSymbols(std::vector<DocumentSymbol> symbols,int64 document,int64 version) {
    fSymbols=std::move(symbols);fDocument=document;fVersion=version;fCurrent=-1;Invalidate();
}
void SymbolBar::SetStatus(const std::string& status,const std::string& detail) { fStatus=status;fDetail=detail;SetToolTip((detail.empty()?status:detail).c_str());Invalidate(); }
void SymbolBar::SetPosition(size_t offset) {
    int selected=-1;size_t length=SIZE_MAX;
    for(size_t i=0;i<fSymbols.size();++i) {
        const auto& symbol=fSymbols[i];
        if(offset>=symbol.start && offset<=symbol.end && symbol.end-symbol.start<=length) { selected=i;length=symbol.end-symbol.start; }
    }
    if(selected!=fCurrent) { fCurrent=selected;Invalidate(); }
}
void SymbolBar::Draw(BRect) {
    auto bounds=Bounds();SetHighColor(fTheme.toolbar);FillRect(bounds);SetLowColor(fTheme.toolbar);
    SetHighColor(IsFocus()?fTheme.accent:fTheme.border);StrokeLine(bounds.LeftBottom(),bounds.RightBottom());
    std::string current="Symbols ▾";
    if(fCurrent>=0) { const auto& symbol=fSymbols[fCurrent];current+="   ";if(!symbol.container.empty()) current+=symbol.container+" › ";current+=symbol.name; }
    BString label(current.c_str());float statusWidth=std::min(280.f,StringWidth(fStatus.c_str()));
    TruncateString(&label,B_TRUNCATE_MIDDLE,std::max(80.f,bounds.Width()-statusWidth-35));
    SetHighColor(fSymbols.empty()?fTheme.muted:fTheme.text);DrawString(label.String(),BPoint(10,19));
    BString status(fStatus.c_str());TruncateString(&status,B_TRUNCATE_END,statusWidth);SetHighColor(fTheme.muted);
    DrawString(status.String(),BPoint(bounds.right-statusWidth-10,19));
}
void SymbolBar::MouseDown(BPoint) { Browse(); }
void SymbolBar::KeyDown(const char* bytes,int32 count) { if(count && (bytes[0]==' ' || bytes[0]==B_ENTER || bytes[0]==B_DOWN_ARROW)) Browse();else BView::KeyDown(bytes,count); }
void SymbolBar::Browse() {
    BPopUpMenu menu("Symbols in current file");
    if(fSymbols.empty()) { auto* empty=new BMenuItem(fStatus.c_str(),nullptr);empty->SetEnabled(false);menu.AddItem(empty); }
    for(size_t i=0;i<fSymbols.size();++i) {
        const auto& symbol=fSymbols[i];std::string title(symbol.depth*2,' ');title+=symbol.name+"  ·  "+SymbolKindName(symbol.kind);
        if(symbol.depth==0 && !symbol.container.empty()) title+="  ("+symbol.container+")";
        auto* message=new BMessage(kSymbolChosen);message->AddInt64("document",fDocument);message->AddInt64("version",fVersion);message->AddInt32("symbol",i);
        auto* item=new BMenuItem(title.c_str(),message);item->SetMarked(static_cast<int>(i)==fCurrent);menu.AddItem(item);
    }
    menu.AddSeparatorItem();menu.AddItem(new BMenuItem("Language Tools…",new BMessage(kShowLanguageTools)));
    menu.SetTargetForItems(Window());menu.Go(ConvertToScreen(BPoint(0,Bounds().bottom+1)),true,true);
}
}
