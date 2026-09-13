#include "ui/TerminalView.h"
#include "ui/Messages.h"
#include <Clipboard.h>
#include <Font.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <Window.h>
#include <algorithm>
#include <cmath>
namespace kiri {
TerminalView::TerminalView():BView("terminal",B_WILL_DRAW|B_FRAME_EVENTS|B_NAVIGABLE) {
    SetExplicitMinSize(BSize(180,80));
    BFont font(be_fixed_font);font.SetSize(12);SetFont(&font);
    font_height metrics;font.GetHeight(&metrics);fCellWidth=ceilf(font.StringWidth("M"));
    fAscent=ceilf(metrics.ascent);fCellHeight=ceilf(metrics.ascent+metrics.descent+metrics.leading)+2;
    ApplyTheme(fTheme);
}
TerminalView::~TerminalView() { fTimer.reset();Stop(); }
void TerminalView::AttachedToWindow() {
    BView::AttachedToWindow();
    BMessage tick(kTerminalTick);fTimer=std::make_unique<BMessageRunner>(BMessenger(this),&tick,16000);
    FrameResized(Bounds().Width(),Bounds().Height());
}
void TerminalView::Stop() {
    fStop=true;if(fWorker.joinable()) fWorker.join();fSession.Stop();
}
void TerminalView::Start(const std::string& directory) {
    Stop();fDirectory=directory;fStop=false;fExited=false;fExitReported=false;
    fModel.Reset();fScrollOffset=0;fSelectionStart=fSelectionEnd=-1;ApplyTheme(fTheme);
    { std::lock_guard<std::mutex> guard(fMutex);fIncoming.clear();fOutgoing.clear(); }
    auto error=fSession.Start(directory,fModel.Rows(),fModel.Columns());
    if(!error.empty()) {
        std::string text="\r\nCannot start terminal: "+error+"\r\n";fModel.Feed(text.data(),text.size());fExited=true;Invalidate();return;
    }
    fWorker=std::thread([this] {
        char buffer[32768];
        while(!fStop) {
            std::string outbound;size_t buffered;
            {
                std::lock_guard<std::mutex> guard(fMutex);
                buffered=fIncoming.size();
                outbound=fOutgoing.substr(0,65536);fOutgoing.erase(0,outbound.size());
            }
            if(!outbound.empty()) fSession.Write(outbound.data(),outbound.size());
            if(buffered<4*1024*1024) {
                auto n=fSession.Read(buffer,sizeof(buffer),20);
                if(n<0) break;
                if(n>0) { std::lock_guard<std::mutex> guard(fMutex);fIncoming.append(buffer,n); }
            } else std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        fExited=true;
    });
}
void TerminalView::FrameResized(float width,float height) {
    int columns=std::max(2,static_cast<int>((width-16)/fCellWidth));
    int rows=std::max(1,static_cast<int>((height-12)/fCellHeight));
    if(columns!=fModel.Columns() || rows!=fModel.Rows()) { fModel.Resize(rows,columns);fSession.Resize(rows,columns);Invalidate(); }
}
void TerminalView::ApplyTheme(const Theme& t) {
    fTheme=t;SetViewColor(t.background);SetLowColor(t.background);
    auto packed=[](rgb_color c){return (c.red<<16)|(c.green<<8)|c.blue;};
    fModel.Colors(packed(t.text),packed(t.background));Invalidate();
}
void TerminalView::FlushInput() {
    auto bytes=fModel.TakeOutput();
    if(!bytes.empty()) { std::lock_guard<std::mutex> guard(fMutex);fOutgoing+=bytes; }
}
void TerminalView::Tick() {
    std::string input;
    { std::lock_guard<std::mutex> guard(fMutex);input=fIncoming.substr(0,256*1024);fIncoming.erase(0,input.size()); }
    if(!input.empty()) { fModel.Feed(input.data(),input.size());FlushInput();Invalidate(); }
    if(fExited && !fExitReported) { fExitReported=true;Window()->PostMessage(kTerminalState);Invalidate(); }
}
void TerminalView::Draw(BRect update) {
    SetHighColor(fTheme.background);FillRect(update);
    int first=std::max(0,static_cast<int>((update.top-6)/fCellHeight));
    int last=std::min(fModel.Rows()-1,static_cast<int>((update.bottom-6)/fCellHeight));
    for(int row=first;row<=last;++row) {
        for(int col=0;col<fModel.Columns();++col) {
            auto cell=fModel.Cell(row,col,fScrollOffset);
            VTermColor fg=cell.fg,bg=cell.bg;
            vterm_screen_convert_color_to_rgb(fModel.Screen(),&fg);vterm_screen_convert_color_to_rgb(fModel.Screen(),&bg);
            rgb_color foreground{fg.rgb.red,fg.rgb.green,fg.rgb.blue,255},background{bg.rgb.red,bg.rgb.green,bg.rgb.blue,255};
            if(cell.attrs.reverse) std::swap(foreground,background);
            if(fSelectionStart>=0 && row>=std::min(fSelectionStart,fSelectionEnd) && row<=std::max(fSelectionStart,fSelectionEnd)) background=fTheme.selection;
            BRect rect(8+col*fCellWidth,6+row*fCellHeight,8+(col+std::max(1,int(cell.width)))*fCellWidth-1,6+(row+1)*fCellHeight-1);
            if(cell.chars[0]==UINT32_MAX) continue;
            SetHighColor(background);FillRect(rect);SetLowColor(background);SetHighColor(foreground);
            std::string text;
            for(auto c:cell.chars) { if(!c) break;if(c!=UINT32_MAX) text+=UTF8(c); }
            if(!cell.attrs.conceal && !text.empty()) DrawString(text.c_str(),BPoint(rect.left,rect.top+fAscent));
            if(cell.attrs.underline) StrokeLine(BPoint(rect.left,rect.bottom-2),BPoint(rect.right,rect.bottom-2));
        }
    }
    if(fScrollOffset==0 && fModel.CursorVisible()) {
        auto cursor=fModel.Cursor();SetHighColor(fTheme.accent);
        BRect caret(8+cursor.col*fCellWidth,6+cursor.row*fCellHeight,8+(cursor.col+1)*fCellWidth-1,6+(cursor.row+1)*fCellHeight-1);
        if(IsFocus()) FillRect(BRect(caret.left,caret.top,caret.left+1,caret.bottom));else StrokeRect(caret);
    }
    if(fExited) { SetHighColor(fTheme.muted);DrawString("Shell exited · use + to open a new terminal",BPoint(10,Bounds().bottom-5)); }
}
void TerminalView::KeyDown(const char* bytes,int32 count) {
    if(count<=0) return;
    fScrollOffset=0;fSelectionStart=fSelectionEnd=-1;
    VTermModifier mods=VTERM_MOD_NONE;
    uint32 flags=modifiers();int32 eventModifiers=0;
    if(Window()->CurrentMessage()->FindInt32("modifiers",&eventModifiers)==B_OK) flags=eventModifiers;
    if(flags&B_SHIFT_KEY) mods=VTERM_MOD_SHIFT;
    if(flags&B_CONTROL_KEY) mods=static_cast<VTermModifier>(mods|VTERM_MOD_CTRL);
    int32 raw=0;
    if((flags&B_CONTROL_KEY) && Window()->CurrentMessage()->FindInt32("raw_char",&raw)==B_OK
        && raw>=32 && raw<127) {
        fModel.Character(raw,VTERM_MOD_CTRL);FlushInput();Invalidate();return;
    }
    VTermKey key=VTERM_KEY_NONE;
    if(count==1 && bytes[0]==B_FUNCTION_KEY) {
        int32 code=0;if(Window()->CurrentMessage()->FindInt32("key",&code)==B_OK && code>=B_F1_KEY && code<=B_F12_KEY)
            key=static_cast<VTermKey>(VTERM_KEY_FUNCTION(code-B_F1_KEY+1));
    }
    if(count==1) switch(bytes[0]) {
        case B_ENTER:key=VTERM_KEY_ENTER;break;case B_BACKSPACE:key=VTERM_KEY_BACKSPACE;break;
        case B_TAB:key=VTERM_KEY_TAB;break;case B_ESCAPE:key=VTERM_KEY_ESCAPE;break;
        case B_UP_ARROW:key=VTERM_KEY_UP;break;case B_DOWN_ARROW:key=VTERM_KEY_DOWN;break;
        case B_LEFT_ARROW:key=VTERM_KEY_LEFT;break;case B_RIGHT_ARROW:key=VTERM_KEY_RIGHT;break;
        case B_HOME:key=VTERM_KEY_HOME;break;case B_END:key=VTERM_KEY_END;break;
        case B_INSERT:key=VTERM_KEY_INS;break;case B_DELETE:key=VTERM_KEY_DEL;break;
        case B_PAGE_UP:key=VTERM_KEY_PAGEUP;break;case B_PAGE_DOWN:key=VTERM_KEY_PAGEDOWN;break;
    }
    if(key!=VTERM_KEY_NONE) fModel.Key(key,mods);
    else {
        for(int32 i=0;i<count;) {
            uint32 c=static_cast<unsigned char>(bytes[i++]);int remaining=0;
            if((c&0xe0)==0xc0) { c&=31;remaining=1; }else if((c&0xf0)==0xe0) { c&=15;remaining=2; }else if((c&0xf8)==0xf0) { c&=7;remaining=3; }
            while(remaining-- && i<count) c=(c<<6)|(static_cast<unsigned char>(bytes[i++])&63);
            // Control key events already contain their control character on Haiku.
            fModel.Character(c,c<32?VTERM_MOD_NONE:static_cast<VTermModifier>(mods&~VTERM_MOD_SHIFT));
        }
    }
    FlushInput();Invalidate();
}
void TerminalView::MouseDown(BPoint where) {
    MakeFocus();fSelecting=true;
    fSelectionStart=fSelectionEnd=std::clamp(static_cast<int>((where.y-6)/fCellHeight),0,fModel.Rows()-1);
    SetMouseEventMask(B_POINTER_EVENTS,B_LOCK_WINDOW_FOCUS);Invalidate();
}
void TerminalView::MouseMoved(BPoint where,uint32,const BMessage*) {
    if(fSelecting) { fSelectionEnd=std::clamp(static_cast<int>((where.y-6)/fCellHeight),0,fModel.Rows()-1);Invalidate(); }
}
void TerminalView::MouseUp(BPoint) { fSelecting=false; }
void TerminalView::MakeFocus(bool focus) {
    bool changed=focus && !IsFocus();BView::MakeFocus(focus);Invalidate();
    if(changed && Window()) Window()->PostMessage(kTerminalFocus);
}
void TerminalView::Copy() {
    if(fSelectionStart<0) return;
    auto text=fModel.Text(std::min(fSelectionStart,fSelectionEnd),std::max(fSelectionStart,fSelectionEnd),fScrollOffset);
    if(be_clipboard->Lock()) { be_clipboard->Clear();be_clipboard->Data()->AddData("text/plain",B_MIME_TYPE,text.data(),text.size());be_clipboard->Commit();be_clipboard->Unlock(); }
}
void TerminalView::Paste() {
    if(be_clipboard->Lock()) {
        const void* bytes;ssize_t size;
        if(be_clipboard->Data()->FindData("text/plain",B_MIME_TYPE,&bytes,&size)==B_OK && size<=16*1024*1024)
            fModel.Paste(std::string(static_cast<const char*>(bytes),size));
        be_clipboard->Unlock();FlushInput();
    }
}
void TerminalView::MessageReceived(BMessage* message) {
    if(message->what==kTerminalTick) Tick();
    else if(message->what==B_COPY) Copy();
    else if(message->what==B_PASTE) Paste();
    else if(message->what==B_MOUSE_WHEEL_CHANGED) {
        float dy=0;message->FindFloat("be:wheel_delta_y",&dy);
        fScrollOffset=std::clamp(fScrollOffset-static_cast<int>(dy*3),0,static_cast<int>(fModel.ScrollbackSize()));Invalidate();
    } else BView::MessageReceived(message);
}
}
