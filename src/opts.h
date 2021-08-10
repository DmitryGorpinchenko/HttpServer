#ifndef OPTS_H
#define OPTS_H

#include <unistd.h>

struct Opts
{
    std::string dir;
    std::string ip;
    short port;

    static Opts &Instance()
    {
        static Opts opts;
        return opts;
    }
    
    void Reset(int argc, char **argv)
    {
        int opt;
        while ((opt = getopt(argc, argv, "h:p:d:")) != -1) {
            switch (opt) {
            case 'h': ip = optarg;              break;
            case 'p': port = std::stoi(optarg); break;
            case 'd': dir = optarg;             break;
            }
        }
    }
private:
    Opts() = default;
};

#endif
