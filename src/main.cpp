#include "ui/Application.h"
#include <signal.h>
int main() { signal(SIGPIPE,SIG_IGN);kiri::Application app;app.Run();return 0; }
