// Exercise the production startup flow using isolated settings and signature.
#include "ui/Application.h"
#include <cstdio>
#include <signal.h>
class LauncherApplication:public kiri::Application {
public:
    LauncherApplication(const char* settings,const char* signature):Application(settings,signature) {}
    void ArgvReceived(int32 argc,char** argv) override { if(argc>3) Application::ArgvReceived(argc-2,argv+2); }
};
int main(int argc,char** argv) {
    if(argc<3) { std::fprintf(stderr,"Usage: kiri_launcher_smoke SETTINGS_DIRECTORY SIGNATURE [PATHS…]\n");return 1; }
    signal(SIGPIPE,SIG_IGN);LauncherApplication application(argv[1],argv[2]);application.Run();return 0;
}
