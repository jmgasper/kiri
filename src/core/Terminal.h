#pragma once
#include <vterm.h>
#include <atomic>
#include <deque>
#include <string>
#include <vector>
#include <sys/types.h>

namespace kiri {
// The model is owned by the UI looper. Only PTY byte transport runs off-thread.
class TerminalModel {
public:
    TerminalModel(int rows=24,int columns=80,size_t scrollback=10000);
    ~TerminalModel();
    TerminalModel(const TerminalModel&)=delete;
    TerminalModel& operator=(const TerminalModel&)=delete;
    void Feed(const char* bytes,size_t size);
    void Reset();
    void Resize(int rows,int columns);
    VTermScreenCell Cell(int row,int column,int scrollOffset=0) const;
    std::string Text(int firstRow,int lastRow,int scrollOffset=0) const;
    std::string TakeOutput();
    void Key(VTermKey key,VTermModifier modifiers=VTERM_MOD_NONE);
    void Character(uint32_t codepoint,VTermModifier modifiers=VTERM_MOD_NONE);
    void Paste(const std::string& text);
    void Colors(uint32_t foreground,uint32_t background);
    VTermPos Cursor() const;
    VTerm* Handle() const { return fTerm; }
    VTermScreen* Screen() const { return fScreen; }
    int Rows() const { return fRows; }
    int Columns() const { return fColumns; }
    size_t ScrollbackSize() const { return fScrollback.size(); }
    bool CursorVisible() const { return fCursorVisible; }
    bool AlternateScreen() const { return fAlternate; }
private:
    static int PushLine(int cols,const VTermScreenCell* cells,void* data);
    static int PopLine(int cols,VTermScreenCell* cells,void* data);
    static int Property(VTermProp prop,VTermValue* value,void* data);
    static int MoveCursor(VTermPos,VTermPos,int visible,void* data);
    VTerm* fTerm;
    VTermScreen* fScreen;
    int fRows,fColumns;
    size_t fScrollbackLimit;
    std::deque<std::vector<VTermScreenCell>> fScrollback;
    std::string fOutput;
    bool fCursorVisible=true,fAlternate=false;
};
class PtySession {
public:
    ~PtySession();
    std::string Start(const std::string& directory,int rows,int columns);
    void Resize(int rows,int columns);
    ssize_t Read(char* buffer,size_t size,int timeoutMs=50);
    bool Write(const char* buffer,size_t size);
    void Stop();
    bool Running();
    pid_t Process() const { return fPid; }
private:
    int fMaster=-1;
    pid_t fPid=-1;
};
std::string UTF8(uint32_t codepoint);
}
