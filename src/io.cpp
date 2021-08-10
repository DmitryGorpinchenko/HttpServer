#include "io.h"

#include <atomic>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

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

//

Socket::Socket()
    : fd(-1)
    , ctl(nullptr)
{
}

Socket::Socket(int _fd)
    : fd(_fd)
    , ctl((_fd >= 0) ? new CtlBlock : nullptr)
{
}

Socket::~Socket()
{
    if ((fd >= 0) && (--ctl->ref_cnt == 0)) {
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
	
    Impl(Socket _s);
};

BufReader::Impl::Impl(Socket _s)
    : s(std::move(_s))
    , cur(0)
    , remaining(0)
{
}

//

BufReader::BufReader(Socket s)
    : pimpl(new Impl(std::move(s)))
{
}

BufReader::~BufReader() = default;

int BufReader::ReadChar()
{
    if (pimpl->remaining == 0) {
        if ((pimpl->remaining = read(pimpl->s, pimpl->buf, sizeof(pimpl->buf))) > 0) {
            pimpl->cur = 0;
        } else {
            return -1;
        }
    }
    return pimpl->buf[--pimpl->remaining, pimpl->cur++];
}

std::string BufReader::ReadLine()
{
    std::string line;
    do {
        int c;
        if ((c = ReadChar()) < 0) {
            break;
        }
        line += c;
        if (c == '\n') {
            break;
        }
    } while (true);
    return line;
}

}
