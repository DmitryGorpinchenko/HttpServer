#ifndef OPTS_H
#define OPTS_H

#include <unistd.h>

struct Opts
{
    std::string dir;
    std::string ip;
    std::string log;
    short port;

    static Opts &Instance();
    void Reset(int argc, char **argv);
private:
    Opts() = default;
};

Opts &Opts::Instance()
{
    static Opts opts;
    return opts;
}

void Opts::Reset(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "h:p:d:l:")) != -1) {
        switch (opt) {
        case 'h': ip = optarg;              break;
        case 'p': port = std::stoi(optarg); break;
        case 'd': dir = optarg;             break;
        case 'l': log = optarg;             break;
        }
    }
}

#endif
