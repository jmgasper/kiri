#include "core/Terminal.h"
#include "core/Process.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <mutex>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

extern char** environ;
namespace kiri {
std::string UTF8(uint32_t c) {
    std::string s;
    if(c<0x80) s+=static_cast<char>(c);
    else if(c<0x800) { s+=static_cast<char>(0xc0|(c>>6));s+=static_cast<char>(0x80|(c&63)); }
    else if(c<0x10000) { s+=static_cast<char>(0xe0|(c>>12));s+=static_cast<char>(0x80|((c>>6)&63));s+=static_cast<char>(0x80|(c&63)); }
    else if(c<=0x10ffff) { s+=static_cast<char>(0xf0|(c>>18));s+=static_cast<char>(0x80|((c>>12)&63));s+=static_cast<char>(0x80|((c>>6)&63));s+=static_cast<char>(0x80|(c&63)); }
    return s;
}
TerminalModel::TerminalModel(int rows,int columns,size_t scrollback)
    :fTerm(vterm_new(rows,columns)),fScreen(vterm_obtain_screen(fTerm)),
    fRows(rows),fColumns(columns),fScrollbackLimit(scrollback) {
    vterm_set_utf8(fTerm,1);
    vterm_screen_enable_altscreen(fScreen,1);
    vterm_screen_enable_reflow(fScreen,true);
    static const VTermScreenCallbacks callbacks=[] {
        VTermScreenCallbacks c{};c.sb_pushline=PushLine;c.sb_popline=PopLine;
        c.settermprop=Property;c.movecursor=MoveCursor;
        c.sb_clear=[](void* data) { static_cast<TerminalModel*>(data)->fScrollback.clear();return 1; };
        return c;
    }();
    vterm_screen_set_callbacks(fScreen,&callbacks,this);
    vterm_output_set_callback(fTerm,[](const char* data,size_t size,void* user) {
        static_cast<TerminalModel*>(user)->fOutput.append(data,size);
    },this);
    vterm_screen_set_damage_merge(fScreen,VTERM_DAMAGE_ROW);
    vterm_screen_reset(fScreen,1);
}
TerminalModel::~TerminalModel() { vterm_free(fTerm); }
void TerminalModel::Reset() { vterm_screen_reset(fScreen,1);fScrollback.clear();fOutput.clear();fAlternate=false;fCursorVisible=true; }
void TerminalModel::Feed(const char* bytes,size_t size) { vterm_input_write(fTerm,bytes,size);vterm_screen_flush_damage(fScreen); }
void TerminalModel::Resize(int rows,int columns) {
    fRows=std::max(1,rows);fColumns=std::max(1,columns);
    vterm_set_size(fTerm,fRows,fColumns);vterm_screen_flush_damage(fScreen);
}
int TerminalModel::PushLine(int cols,const VTermScreenCell* cells,void* data) {
    auto& self=*static_cast<TerminalModel*>(data);
    self.fScrollback.emplace_back(cells,cells+cols);
    if(self.fScrollback.size()>self.fScrollbackLimit) self.fScrollback.pop_front();
    return 1;
}
int TerminalModel::PopLine(int cols,VTermScreenCell* cells,void* data) {
    auto& self=*static_cast<TerminalModel*>(data);
    if(self.fScrollback.empty()) return 0;
    const auto& line=self.fScrollback.back();
    std::fill(cells,cells+cols,VTermScreenCell{});
    std::copy_n(line.begin(),std::min<size_t>(cols,line.size()),cells);
    self.fScrollback.pop_back();return 1;
}
int TerminalModel::Property(VTermProp prop,VTermValue* value,void* data) {
    auto& self=*static_cast<TerminalModel*>(data);
    if(prop==VTERM_PROP_CURSORVISIBLE) self.fCursorVisible=value->boolean;
    if(prop==VTERM_PROP_ALTSCREEN) self.fAlternate=value->boolean;
    return 1;
}
int TerminalModel::MoveCursor(VTermPos,VTermPos,int visible,void* data) {
    static_cast<TerminalModel*>(data)->fCursorVisible=visible;return 1;
}
VTermScreenCell TerminalModel::Cell(int row,int column,int scrollOffset) const {
    VTermScreenCell cell{};
    const int actual=row-std::min<int>(scrollOffset,fScrollback.size());
    if(actual<0) {
        int index=static_cast<int>(fScrollback.size())+actual;
        if(index>=0 && column>=0 && column<static_cast<int>(fScrollback[index].size())) cell=fScrollback[index][column];
    } else if(actual<fRows && column>=0 && column<fColumns) vterm_screen_get_cell(fScreen,{actual,column},&cell);
    // Default colors follow the current theme, including cells in scrollback.
    // Explicit RGB and indexed terminal colors retain their original meaning.
    if(VTERM_COLOR_IS_DEFAULT_FG(&cell.fg)) {vterm_color_rgb(&cell.fg,fForeground>>16,(fForeground>>8)&255,fForeground&255);cell.fg.type|=VTERM_COLOR_DEFAULT_FG;}
    if(VTERM_COLOR_IS_DEFAULT_BG(&cell.bg)) {vterm_color_rgb(&cell.bg,fBackground>>16,(fBackground>>8)&255,fBackground&255);cell.bg.type|=VTERM_COLOR_DEFAULT_BG;}
    return cell;
}
std::string TerminalModel::Text(int firstRow,int lastRow,int scrollOffset) const {
    std::string result;
    for(int row=firstRow;row<=lastRow;++row) {
        std::string line;
        for(int column=0;column<fColumns;++column) {
            auto cell=Cell(row,column,scrollOffset);
            if(cell.chars[0]==UINT32_MAX) continue;
            if(!cell.chars[0]) line+=' ';
            for(uint32_t c:cell.chars) { if(!c) break;if(c!=UINT32_MAX) line+=UTF8(c); }
        }
        while(!line.empty() && line.back()==' ') line.pop_back();
        result+=line;if(row!=lastRow) result+='\n';
    }
    return result;
}
std::string TerminalModel::TakeOutput() { auto out=std::move(fOutput);fOutput.clear();return out; }
void TerminalModel::Key(VTermKey key,VTermModifier mod) { vterm_keyboard_key(fTerm,key,mod); }
void TerminalModel::Character(uint32_t cp,VTermModifier mod) { vterm_keyboard_unichar(fTerm,cp,mod); }
void TerminalModel::Paste(const std::string& text) {
    vterm_keyboard_start_paste(fTerm);
    // Preserve UTF-8 and newlines; bracketed paste markers come from libvterm.
    fOutput+=text;
    vterm_keyboard_end_paste(fTerm);
}
void TerminalModel::Colors(uint32_t foreground,uint32_t background) {
    fForeground=foreground;fBackground=background;
    VTermColor fg,bg;
    vterm_color_rgb(&fg,foreground>>16,(foreground>>8)&255,foreground&255);
    vterm_color_rgb(&bg,background>>16,(background>>8)&255,background&255);
    vterm_state_set_default_colors(vterm_obtain_state(fTerm),&fg,&bg);
}
void TerminalModel::Palette(const std::array<uint32_t,16>& colors) {
    auto* state=vterm_obtain_state(fTerm);
    for(int i=0;i<16;++i) {VTermColor color;auto c=colors[i];vterm_color_rgb(&color,c>>16,(c>>8)&255,c&255);vterm_state_set_palette_color(state,i,&color);}
}
VTermPos TerminalModel::Cursor() const { VTermPos p{};vterm_state_get_cursorpos(vterm_obtain_state(fTerm),&p);return p; }
PtySession::~PtySession() { Stop(); }
std::string PtySession::Start(const std::string& directory,int rows,int columns) {
    Stop();
    fMaster=posix_openpt(O_RDWR|O_NOCTTY|O_CLOEXEC);
    if(fMaster<0) return strerror(errno);
    if(grantpt(fMaster)!=0 || unlockpt(fMaster)!=0) { auto error=std::string(strerror(errno));Stop();return error; }
    char slave[256];
#ifdef __HAIKU__
    {
        // R1/beta5 exposes ptsname(), but not the later ptsname_r(). Copy its
        // shared buffer before starting another terminal session.
        static std::mutex nameMutex;std::lock_guard<std::mutex> guard(nameMutex);
        const char* name=ptsname(fMaster);
        if(!name || strlen(name)>=sizeof(slave)) { Stop();return "Cannot find PTY slave."; }
        strcpy(slave,name);
    }
#else
    if(ptsname_r(fMaster,slave,sizeof(slave))!=0) { Stop();return "Cannot find PTY slave."; }
#endif
    std::string shell=getenv("SHELL")?getenv("SHELL"):"/bin/bash";
    if(access(shell.c_str(),X_OK)!=0) shell="/boot/system/bin/bash";
    std::vector<std::string> env;
    for(char** e=environ;*e;++e) {
        if(strncmp(*e,"TERM=",5)!=0 && strncmp(*e,"COLORTERM=",10)!=0) env.emplace_back(*e);
    }
    env.emplace_back("TERM=xterm-256color");env.emplace_back("COLORTERM=truecolor");
    std::vector<char*> envp;
    for(auto& e:env) envp.push_back(e.data());
    envp.push_back(nullptr);
    char* args[]={shell.data(),const_cast<char*>("-i"),nullptr};
    int ready[2];
    if(pipe(ready)!=0) { auto error=std::string(strerror(errno));Stop();return error; }
    fcntl(ready[0],F_SETFD,FD_CLOEXEC);fcntl(ready[1],F_SETFD,FD_CLOEXEC);
    fPid=fork();
    if(fPid==0) {
        close(ready[0]);
        auto fail=[&] { int code=errno;write(ready[1],&code,sizeof(code));_exit(126); };
        close(fMaster);
        if(setsid()<0) fail();
        int fd=open(slave,O_RDWR);
        if(fd<0) fail();
#ifdef TIOCSCTTY
        ioctl(fd,TIOCSCTTY,0);
#endif
        struct termios mode{};
        tcgetattr(fd,&mode);
        mode.c_iflag=ICRNL|IXON;mode.c_oflag=OPOST|ONLCR;
        mode.c_cflag=(mode.c_cflag&~(CSIZE|CSTOPB|PARENB))|CS8|CREAD|CLOCAL;
        cfsetispeed(&mode,B38400);cfsetospeed(&mode,B38400);
        mode.c_lflag=ISIG|ICANON|ECHO|ECHOE|ECHOK|IEXTEN;
        // Haiku aliases VMIN/VEOF and VTIME/VEOL. Install the canonical
        // control characters last so Ctrl-D remains EOF instead of Ctrl-A.
        mode.c_cc[VMIN]=1;mode.c_cc[VTIME]=0;
        mode.c_cc[VINTR]=3;mode.c_cc[VQUIT]=28;mode.c_cc[VERASE]=127;
        mode.c_cc[VKILL]=21;mode.c_cc[VEOF]=4;mode.c_cc[VSTART]=17;mode.c_cc[VSTOP]=19;
        mode.c_cc[VSUSP]=26;
        tcsetattr(fd,TCSANOW,&mode);
        struct winsize size{};size.ws_row=rows;size.ws_col=columns;ioctl(fd,TIOCSWINSZ,&size);
        dup2(fd,0);dup2(fd,1);dup2(fd,2);if(fd>2) close(fd);
        for(int sig:{SIGCHLD,SIGHUP,SIGINT,SIGQUIT,SIGTERM,SIGTTOU,SIGTTIN,SIGTSTP,SIGPIPE}) signal(sig,SIG_DFL);
        sigset_t mask;sigemptyset(&mask);sigprocmask(SIG_SETMASK,&mask,nullptr);
        if(!directory.empty() && chdir(directory.c_str())!=0) fail();
        execve(shell.c_str(),args,envp.data());fail();
    }
    close(ready[1]);
    if(fPid<0) { auto error=std::string(strerror(errno));close(ready[0]);Stop();return error; }
    // On Haiku the master rejects reads/writes until the child opens its slave.
    // EOF on this close-on-exec pipe proves that the shell has reached exec.
    pollfd handshake{ready[0],POLLIN|POLLHUP,0};
    int waitResult;
    do { waitResult=poll(&handshake,1,5000); } while(waitResult<0 && errno==EINTR);
    int childError=0;ssize_t received=waitResult>0?read(ready[0],&childError,sizeof(childError)):-1;
    close(ready[0]);
    if(received!=0) { std::string error=received>0?strerror(childError):"Shell startup timed out.";Stop();return error; }
    fcntl(fMaster,F_SETFL,fcntl(fMaster,F_GETFL)|O_NONBLOCK);
    return {};
}
void PtySession::Resize(int rows,int columns) {
    if(fMaster<0) return;
    struct winsize size{};size.ws_row=rows;size.ws_col=columns;ioctl(fMaster,TIOCSWINSZ,&size);
}
ssize_t PtySession::Read(char* buffer,size_t size,int timeoutMs) {
    if(fMaster<0) return -1;
    pollfd descriptor{fMaster,POLLIN,0};
    int ready=poll(&descriptor,1,timeoutMs);
    if(ready==0 || (ready<0 && errno==EINTR)) return 0;
    if(ready<0) return -1;
    ssize_t n=read(fMaster,buffer,size);
    if(n<0 && (errno==EAGAIN || errno==EINTR)) return 0;
    return n==0?-1:n;
}
bool PtySession::Write(const char* buffer,size_t size) {
    if(fMaster<0) return false;
    size_t offset=0;
    while(offset<size) {
        ssize_t n=write(fMaster,buffer+offset,size-offset);
        if(n>0) offset+=n;
        else if(n<0 && errno==EINTR) continue;
        else if(n<0 && errno==EAGAIN) {
            pollfd descriptor{fMaster,POLLOUT,0};if(poll(&descriptor,1,100)<=0) return false;
        } else return false;
    }
    return true;
}
bool PtySession::Running() {
    if(fPid<0) return false;
    int status=0;
    if(waitpid(fPid,&status,WNOHANG)==fPid) { fPid=-1;return false; }
    return true;
}
void PtySession::Stop() {
    if(fPid>0) {
        // Closing the PTY sends a hangup to the foreground job as well as the shell.
        kill(-fPid,SIGHUP);kill(fPid,SIGHUP);
        if(fMaster>=0) { close(fMaster);fMaster=-1; }
        int status=0;
        for(int i=0;i<20;++i) {
            if(waitpid(fPid,&status,WNOHANG)==fPid) { fPid=-1;break; }
            poll(nullptr,0,10);
        }
        if(fPid>0) { kill(-fPid,SIGKILL);kill(fPid,SIGKILL);ReapKilledChild(fPid,status); }
        fPid=-1;
    }
    if(fMaster>=0) { close(fMaster);fMaster=-1; }
}
}
