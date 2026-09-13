#pragma once
#include "core/Terminal.h"
#include "ui/Theme.h"
#include <View.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
class BMessageRunner;
namespace kiri {
class TerminalView:public BView {
public:
    TerminalView();
    ~TerminalView() override;
    void AttachedToWindow() override;
    void FrameResized(float width,float height) override;
    void Draw(BRect update) override;
    void KeyDown(const char* bytes,int32 count) override;
    void MouseDown(BPoint where) override;
    void MouseMoved(BPoint where,uint32 transit,const BMessage* drag) override;
    void MouseUp(BPoint where) override;
    void MessageReceived(BMessage* message) override;
    void MakeFocus(bool focus=true) override;
    void Start(const std::string& directory);
    void ApplyTheme(const Theme& theme);
private:
    void Stop();
    void FlushInput();
    void Tick();
    void Copy();
    void Paste();
    TerminalModel fModel;
    PtySession fSession;
    std::unique_ptr<BMessageRunner> fTimer;
    std::thread fWorker;
    std::atomic<bool> fStop{false},fExited{false};
    std::mutex fMutex;
    std::string fIncoming,fOutgoing;
    std::string fDirectory;
    float fCellWidth=8,fCellHeight=18,fAscent=13;
    int fScrollOffset=0,fSelectionStart=-1,fSelectionEnd=-1;
    bool fSelecting=false;
    Theme fTheme=Theme::Builtins()[0];
};
}
