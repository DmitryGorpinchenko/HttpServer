#include "io.h"

#include <atomic>
#include <string>
#include <mutex>
#include <fstream>

#include <unistd.h>

namespace IO {

struct Socket::CtlBlock
{
    std::atomic<size_t> ref_cnt;

    CtlBlock();
};

Socket::CtlBlock::CtlBlock()
    : ref_cnt(1)
{
}

Socket::Socket()
    : fd(-1)
    , ctl(nullptr)
{
}

Socket::Socket(int _fd)
    : fd(_fd)
    , ctl((_fd >= 0) ? new CtlBlock : nullptr)
{
    if (fd >= 0) {
        Logger::Instance().Log("  Socket " + std::to_string(fd) + ": Opened");
    }
}

Socket::~Socket()
{
    if ((fd >= 0) && (--ctl->ref_cnt == 0)) {
        Logger::Instance().Log("  Socket " + std::to_string(fd) + ": Closed");

        close(fd);
        fd = -1;

        delete ctl;
        ctl = nullptr;
    }
}

Socket::Socket(const Socket &rhs)
    : fd(rhs.fd)
    , ctl(rhs.ctl)
{
    if (ctl) {
        ++ctl->ref_cnt;
    }
}

Socket &Socket::operator =(const Socket &rhs)
{
    if (this != &rhs) {
        Socket tmp(rhs);
        Swap(tmp);
    }
    return *this;
}

Socket::Socket(Socket &&rhs)
    : fd(rhs.fd)
    , ctl(rhs.ctl)
{
    rhs.fd = -1;
    rhs.ctl = nullptr;
}

Socket &Socket::operator =(Socket &&rhs)
{
    if (this != &rhs) {
        Socket tmp(std::move(rhs));
        Swap(tmp);
    }
    return *this;
}

Socket::operator bool() const
{
    return fd >= 0;
}

Socket::operator int() const
{
    return fd;
}

void Socket::Swap(Socket &other)
{
    std::swap(fd, other.fd);
    std::swap(ctl, other.ctl);
}

//

struct BufReader::Impl
{
    Socket s;
    char buf[1024];
    int cur;
    int remaining;
    bool eof;
	
    Impl(Socket _s);
};

BufReader::Impl::Impl(Socket _s)
    : s(std::move(_s))
    , cur(-1)
    , remaining(0)
    , eof(false)
{
}

//

static const auto c_invalid_char = std::char_traits<char>::eof();

BufReader::BufReader(Socket s)
    : pimpl(new Impl(std::move(s)))
{
}

BufReader::~BufReader() = default;

int BufReader::ReadChar()
{
    if (pimpl->eof) {
        return c_invalid_char;
    }
    if (pimpl->remaining == 0) {
        const auto n = read(pimpl->s, pimpl->buf, sizeof(pimpl->buf));
        if (n > 0) {
            pimpl->remaining = n;
            pimpl->cur = 0;
        } else {
            if (n == 0) {
                pimpl->eof = true;
            }
            return c_invalid_char;
        }
    }
    return pimpl->buf[--pimpl->remaining, pimpl->cur++];
}

std::string BufReader::ReadLine()
{
    std::string line;
    do {
        const int c = ReadChar();
        if (c == c_invalid_char) {
            break;
        }
        line += c;
        if (c == '\n') {
            break;
        }
    } while (true);
    return line;
}

bool BufReader::Eof() const
{
    return pimpl->eof;
}

//

struct Logger::Impl
{
    std::mutex m;
    std::ofstream fs;

    Impl(const std::string &path);
};

Logger::Impl::Impl(const std::string &path)
    : fs(path)
{
}

Logger &Logger::Instance()
{
    static Logger log;
    return log;
}

void Logger::Reset(const std::string &path)
{
    pimpl.reset(new Impl(path));
}

void Logger::Log(const std::string &msg)
{
    std::lock_guard<std::mutex> lock(pimpl->m);
    pimpl->fs << msg << std::endl;
}

}
