#include "http_server.h"
#include "opts.h"

#include <signal.h>

int main(int argc, char **argv)
{
    signal(SIGHUP, SIG_IGN);

    if (daemon(0, 0) < 0) {
        return 1;
    }
    
    auto& opts = Opts::Instance();
    opts.Reset(argc, argv);
    
    try {
        Http::Server(opts.ip, opts.port, opts.dir).Run();
    } catch (...) {
        return 1;
    }
    return 0;
}
