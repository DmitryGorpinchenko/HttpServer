#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include <string>

namespace Http {

struct Error
{
};

class Server
{
public:
    explicit Server(const std::string &ip, short port, const std::string &dir);
    ~Server();

    void Run();
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

}

#endif
