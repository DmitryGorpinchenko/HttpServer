#ifndef IO_H
#define IO_H

#include <memory>

namespace IO {

class Socket
{
public:
    Socket();
    explicit Socket(int _fd);
    ~Socket();

    Socket(const Socket &rhs);
    Socket &operator =(const Socket &rhs);

    Socket(Socket &&rhs);
    Socket &operator =(Socket &&rhs);

    operator bool() const;
    operator int() const;
private:
    void Swap(Socket &other);

    int fd;

    struct CtlBlock;
    CtlBlock *ctl;
};

class BufReader
{
public:
    explicit BufReader(Socket s);
    ~BufReader();
    
    BufReader(const BufReader &) = delete;
    BufReader &operator =(const BufReader &) = delete;

    int ReadChar();
    std::string ReadLine();

    bool Eof() const;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

class Logger
{
public:
    static Logger &Instance();
    void Reset(const std::string &path);

    void Log(const std::string &msg);
private:
    Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator =(const Logger &) = delete;

    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

}

#endif
