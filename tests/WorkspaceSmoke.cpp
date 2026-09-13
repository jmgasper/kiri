// A separate application signature and settings directory let native UI checks
// run alongside a user's open Kiri session without touching its documents.
#include "ui/Workspace.h"
#include <Application.h>
#include <cstdio>
#include <signal.h>

int main(int argc,char** argv) {
    if(argc<2) { std::fprintf(stderr,"Usage: kiri_workspace_smoke SETTINGS_DIRECTORY [PROJECT]\n");return 1; }
    signal(SIGPIPE,SIG_IGN);
    BApplication application("application/x-vnd.Kiri-workspace-tests");
    auto* window=new kiri::Workspace(argv[1]);
    if(argc>2) window->OpenProject(argv[2]);
    window->Show();application.Run();
    return 0;
}
