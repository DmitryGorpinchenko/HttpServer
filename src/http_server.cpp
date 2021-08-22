#include "http_server.h"
#include "worker_pool.h"
#include "io.h"

#include <vector>
#include <thread>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <chrono>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace Http {

namespace {

using TimePoint = std::chrono::steady_clock::time_point;

struct Connection
{
    static const int c_keep_alive_sec = 5;
    static const int c_keep_alive_ms = c_keep_alive_sec * 1000;

    IO::Socket s;
    std::unique_ptr<IO::BufReader> r;
    Concurrent::IWorker *w;
    TimePoint last_active;

    Connection(IO::Socket _s, TimePoint timestamp);

    bool Idle(TimePoint now) const;
    int RemainingMs(TimePoint now) const;

    operator bool() const;
};

struct Acceptor
{
    IO::Socket master;

    Acceptor(const std::string &ip, short port);

    int Bind(const std::string &ip, short port);
    int Listen();

    Connection Accept(TimePoint timestamp);
};

struct Poller
{
    static const int c_max_events = 32;

    int epoll;
    epoll_event events[c_max_events];
    int ret_events;
    TimePoint timestamp;

    std::vector<Connection> conns;

    Poller(const Acceptor &acceptor);

    bool Wait();

    using ConnHdl = std::vector<Connection>::iterator;

    bool Add(Connection c);
    void Remove(ConnHdl c);
    ConnHdl Find(int fd);

    void RemoveAllIdle();
    int TimeoutMs() const;
};

struct Request : Concurrent::ITask
{
    static int64_t count;

    int64_t id;
    IO::Socket s;
    std::string dir;
    std::string request_line;
    bool bad;

    static std::unique_ptr<Request> Read(IO::BufReader &reader, IO::Socket s, const std::string &dir);

    Request(IO::Socket _s, const std::string &_dir, const std::string &_request_line, bool _bad);

    Request(const Request &) = delete;
    Request &operator =(const Request &) = delete;

    void Perform() override;
};

struct Response
{
    static void Send(IO::Socket s, const char *status_code, const char *content_type, size_t content_len, const std::string &content);
};

struct FileMetaData
{
    const char *mime_type;
    bool is_binary;

    FileMetaData(const char *fname);
};

//

Connection::Connection(IO::Socket _s, TimePoint timestamp)
    : s(std::move(_s))
    , r(new IO::BufReader(s))
    , w(nullptr)
    , last_active(timestamp)
{
}

bool Connection::Idle(TimePoint now) const
{
    return RemainingMs(now) == 0;
}

int Connection::RemainingMs(TimePoint now) const
{
    const int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_active).count();
    return (elapsed_ms < c_keep_alive_ms) ? (c_keep_alive_ms - elapsed_ms) : 0;
}

Connection::operator bool() const
{
    return s;
}

//

Acceptor::Acceptor(const std::string &ip, short port)
    : master(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP))
{
    if (!master || Bind(ip, port) < 0 || Listen() < 0) {
        throw Error();
    }
}

int Acceptor::Bind(const std::string &ip, short port)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        return -1;
    }

    return bind(master, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
}

int Acceptor::Listen()
{
    return listen(master, SOMAXCONN);
}

Connection Acceptor::Accept(TimePoint timestamp)
{
    return Connection(IO::Socket(accept4(master, nullptr, nullptr, SOCK_NONBLOCK)), timestamp);
}

//

Poller::Poller(const Acceptor &acceptor)
    : epoll(epoll_create1(0))
    , ret_events(0)
    , timestamp(std::chrono::steady_clock::now())
{
    epoll_event ev;
    bzero(&ev, sizeof(epoll_event));
    ev.events = EPOLLIN;
    ev.data.fd = acceptor.master;

    if (epoll < 0 || epoll_ctl(epoll, EPOLL_CTL_ADD, acceptor.master, &ev) < 0) {
        throw Error();
    }
}

bool Poller::Wait()
{
    ret_events = epoll_wait(epoll, events, c_max_events, TimeoutMs());
    timestamp = std::chrono::steady_clock::now();
    return ret_events >= 0;
}

bool Poller::Add(Connection c)
{
    if (!c) {
        return false;
    }
    epoll_event ev;
    bzero(&ev, sizeof(epoll_event));
    ev.events = EPOLLIN;
    ev.data.fd = c.s;
    const int res = epoll_ctl(epoll, EPOLL_CTL_ADD, c.s, &ev);
    if (res != 0) {
        return false;
    }
    if (Find(c.s) == conns.end()) {
        conns.push_back(std::move(c));
    }
    return true;
}

void Poller::Remove(ConnHdl c)
{
    if (c != conns.end()) {
        std::swap(*c, conns.back());
        conns.pop_back();
    }
}

Poller::ConnHdl Poller::Find(int fd)
{
    return std::find_if(conns.begin(), conns.end(), [fd](const Connection &c) { return int(c.s) == fd; });
}

void Poller::RemoveAllIdle()
{
    conns.erase(std::remove_if(conns.begin(), conns.end(), [this](const Connection &c) { return c.Idle(timestamp); }), conns.end());
}

int Poller::TimeoutMs() const
{
    auto it = std::min_element(conns.begin(), conns.end(), [this](const Connection &lhs, const Connection &rhs) { return lhs.RemainingMs(timestamp) < rhs.RemainingMs(timestamp); });
    return (it != conns.end()) ? (*it).RemainingMs(timestamp) : -1;
}

//

int64_t Request::count = 0;

std::unique_ptr<Request> Request::Read(IO::BufReader &reader, IO::Socket s, const std::string &dir)
{
    const auto request_line = reader.ReadLine();
    if (request_line.empty()) {
        return nullptr;
    }

    bool bad = false;
    do { // ignore request headers
        const auto line = reader.ReadLine();
        if (line == "\r\n") {
            break;
        }
        if (line.empty()) {
            bad = true;
            break;
        }
    } while (true);

    std::unique_ptr<Request> res(new Request(std::move(s), dir, request_line, bad));
    IO::Logger::Instance().Log(" Request " + std::to_string(int(res->s)) + ":" + std::to_string(res->id) + ": " + (request_line.substr(0, request_line.size() - ((request_line.back() == '\n') ? 1 : 0))));
    return res;
}

Request::Request(IO::Socket _s, const std::string &_dir, const std::string &_request_line, bool _bad)
    : id(++count)
    , s(std::move(_s))
    , dir(_dir)
    , request_line(_request_line)
    , bad(_bad)
{
}

void Request::Perform()
{
    if (bad) {
        IO::Logger::Instance().Log("Response " + std::to_string(int(s)) + ":" + std::to_string(id) + ": HTTP/1.1 400 Bad Request");
        Response::Send(s, "400 Bad Request", "text/plain", strlen("Bad Request"), "Bad Request");
        return;
    }

    std::string method, uri, version;
    std::istringstream(request_line) >> method  >> uri >> version;
    if ((method != "GET") && (method != "HEAD")) {
        IO::Logger::Instance().Log("Response " + std::to_string(int(s)) + ":" + std::to_string(id) + ": HTTP/1.1 501 Not Implemented");
        Response::Send(s, "501 Not Implemented", "text/plain", strlen("Not Implemented"), "Not Implemented");
        return;
    }

    const auto fname = dir + uri.substr(0, uri.find_first_of('?'));
    const FileMetaData meta_data(fname.c_str());

    std::ifstream in(fname, meta_data.is_binary ? std::ios_base::binary : std::ios_base::in);
    if (!in) {
        IO::Logger::Instance().Log("Response " + std::to_string(int(s)) + ":" + std::to_string(id) + ": HTTP/1.1 404 Not Found");
        Response::Send(s, "404 Not Found", "text/plain", strlen("Not Found"), "Not Found");
        return;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    const auto str = buffer.str();
    IO::Logger::Instance().Log("Response " + std::to_string(int(s)) + ":" + std::to_string(id) + ": HTTP/1.1 200 OK");
    Response::Send(s, "200 OK", meta_data.mime_type, str.size(), (method == "HEAD") ? std::string() : str);
}

//

void Response::Send(IO::Socket s, const char *status_code, const char *content_type, size_t content_len, const std::string &content)
{
    std::string res = "HTTP/1.1 ";
    res += status_code;
    res += "\r\n";
    res += "Server: HttpServer\r\n";
    res += "Connection: keep-alive\r\n";
    res += "Keep-Alive: timeout=";
    res += std::to_string(Connection::c_keep_alive_sec);
    res += "\r\n";
    res += "Content-type: ";
    res += content_type;
    res += "\r\n";
    res += "X-Content-Type-Options: nosniff\r\n";
    res += "Content-length: ";
    res += std::to_string(content_len);
    res += "\r\n\r\n";
    res += content; // std::string is used for 'content' to allow '\0' character to be in the middle of the buffer

    send(s, res.c_str(), res.size(), MSG_NOSIGNAL);
}

//

FileMetaData::FileMetaData(const char *fname)
{
    if (strstr(fname, ".html")) {
        mime_type = "text/html";
        is_binary = false;
    } else if (strstr(fname, ".css")) {
        mime_type = "text/css";
        is_binary = false;
    } else if (strstr(fname, ".js")) {
        mime_type = "text/javascript";
        is_binary = false;
    } else if (strstr(fname, ".png")) {
        mime_type = "image/png";
        is_binary = true;
    } else if (strstr(fname, ".gif")) {
        mime_type = "image/gif";
        is_binary = true;
    } else if (strstr(fname, ".jpg")) {
        mime_type = "image/jpeg";
        is_binary = true;
    } else if (strstr(fname, ".svg")) {
        mime_type = "image/svg+xml";
        is_binary = true;
    } else if (strstr(fname, ".eot")) {
        mime_type = "application/vnd.ms-fontobject";
        is_binary = true;
    } else if (strstr(fname, ".ttf")) {
        mime_type = "font/ttf";
        is_binary = true;
    } else if (strstr(fname, ".woff")) {
        mime_type = "font/woff";
        is_binary = true;
    } else if (strstr(fname, ".woff2")) {
        mime_type = "font/woff2";
        is_binary = true;
    } else {
        mime_type = "text/plain";
        is_binary = false;
    }
}

} // end namespace

struct Server::Impl
{
    Acceptor acceptor;
    Poller poller;
    std::unique_ptr<Concurrent::WorkerPool> worker_pool;
    std::string dir;

    Impl(const std::string &_ip, short _port, const std::string &_dir);

    void Run();
    void ProcessEvents();
    void CloseIdleConnections();

    void AcceptPendingConnections();
    void ProcessConnection(Poller::ConnHdl c);
};

Server::Impl::Impl(const std::string &_ip, short _port, const std::string &_dir)
    : acceptor(_ip, _port)
    , poller(acceptor)
    , worker_pool(new Concurrent::RoundRobinWorkerPool(std::max(1u, std::thread::hardware_concurrency()) * (1 + 50 /* wait time */ / 5 /* service time */)))
    , dir(_dir)
{
}

void Server::Impl::Run()
{
    worker_pool->Start();

    while (poller.Wait()) {
        ProcessEvents();
        CloseIdleConnections();
    }
}

void Server::Impl::ProcessEvents()
{
    for (int i = 0; i < poller.ret_events; ++i) {
        const auto &ev = poller.events[i];
        if (ev.events & EPOLLIN) {
            if (ev.data.fd == acceptor.master) {
                AcceptPendingConnections();
            } else {
                ProcessConnection(poller.Find(ev.data.fd));
            }
        }
    }
}

void Server::Impl::CloseIdleConnections()
{
    poller.RemoveAllIdle();
}

void Server::Impl::AcceptPendingConnections()
{
    while (poller.Add(acceptor.Accept(poller.timestamp)))
        ;
}

void Server::Impl::ProcessConnection(Poller::ConnHdl c)
{
    if (c == poller.conns.end()) {
        return;
    }
    c->last_active = poller.timestamp;
    do {
        std::unique_ptr<Concurrent::ITask> task(Request::Read(*c->r, c->s, dir));
        const bool eof = c->r->Eof(); // 'true' means socket closed from the client side
        if (task) {
            if (!c->w) { // each connection must have associated worker to properly serialize responses (to pipelined requests)
                c->w = worker_pool->SubmitTask(std::move(task));
            } else {
                c->w->AssignTask(std::move(task));
            }
            if (eof) {
                poller.Remove(c);
                break;
            }
        } else {
            if (eof) {
                poller.Remove(c);
            }
            break;
        }
    } while (true);
}

//

Server::Server(const std::string &ip, short port, const std::string &dir)
    : pimpl(new Impl(ip, port, dir))
{
}

Server::~Server() = default;

void Server::Run()
{
    pimpl->Run();
}

}
