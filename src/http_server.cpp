#include "http_server.h"
#include "io.h"
#include "worker_pool.h"

#include <thread>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace Http {

struct Request : Concurrent::Task
{
    IO::Socket s;
    std::string dir;

    Request(IO::Socket _s, const std::string &_dir);

    void Perform() override;
};

Request::Request(IO::Socket _s, const std::string &_dir)
    : s(std::move(_s))
    , dir(_dir)
{
}

void Request::Perform()
{
    std::string method, uri, version;
    std::istringstream(IO::BufReader(s).ReadLine()) >> method  >> uri >> version;
    const auto file = dir + uri.substr(0, uri.find_first_of('?'));

    const auto response = [](const char *status_code,
                             const char *status_msg,
                             const char *content_type,
                             const std::string &content)
    {
        std::string res = "HTTP/1.1 ";
        res += status_code;
        res += ' ';
        res += status_msg;
        res += "\r\n";
        res += "Server: HttpServer\r\n";
        res += "Connection: close\r\n"; // TODO: remove after persistent connections are implemented
        res += "Content-length: ";
        res += std::to_string(content.size());
        res += "\r\n";
        res += "Content-type: ";
        res += content_type;
        res += "\r\n";
        res += "X-Content-Type-Options: nosniff";
        res += "\r\n\r\n";
        res += content;
        return res;
    };

    std::ifstream in(file);
    if (!in) {
        const auto msg = response("404", "Not found", "text/plain", "Error occured");
        send(s, msg.c_str(), msg.size(), MSG_NOSIGNAL);
        return;
    }

    const auto type = [](const char *fname)
    {
        if (strstr(fname, ".html")) {
            return "text/html";
        } else if (strstr(fname, ".css")) {
            return "text/css";
        } else if (strstr(fname, ".js")) {
            return "text/javascript";
        } else if (strstr(fname, ".png")) {
            return "image/png";
        } else if (strstr(fname, ".gif")) {
            return "image/gif";
        } else if (strstr(fname, ".jpg")) {
            return "image/jpeg";
        } else if (strstr(fname, ".svg")) {
            return "image/svg+xml";
        } else if (strstr(fname, ".eot")) {
            return "application/vnd.ms-fontobject";
        } else if (strstr(fname, ".ttf")) {
            return "font/ttf";
        } else if (strstr(fname, ".woff")) {
            return "font/woff";
        } else if (strstr(fname, ".woff2")) {
            return "font/woff2";
        }
        return "text/plain";
    };

    std::stringstream buffer;
    buffer << in.rdbuf();
    const auto msg = response("200", "OK", type(file.c_str()), buffer.str());
    send(s, msg.c_str(), msg.size(), MSG_NOSIGNAL);
}

//

struct Acceptor
{
    IO::Socket master;

    Acceptor(const std::string &ip, short port);

    int Bind(const std::string &ip, short port);
    int Listen();

    IO::Socket Accept();
};

Acceptor::Acceptor(const std::string &ip, short port)
    : master(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
{
    if (Bind(ip, port) < 0 || Listen() < 0) {
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

IO::Socket Acceptor::Accept()
{
    return IO::Socket(accept(master, nullptr, nullptr));
}

//

struct Server::Impl
{
    Acceptor acceptor;
    std::unique_ptr<Concurrent::WorkerPool> worker_pool;
    std::string dir;

    Impl(const std::string &_ip, short _port, const std::string &_dir);

    void Run();
    void AcceptRequest();
};

Server::Impl::Impl(const std::string &_ip, short _port, const std::string &_dir)
    : acceptor(_ip, _port)
    , worker_pool(new Concurrent::RoundRobinWorkerPool(std::max(1u, std::thread::hardware_concurrency() * (1 + 50 /* wait time */ / 5 /* service time */))))
    , dir(_dir)
{
}

void Server::Impl::Run()
{
    worker_pool->Start();

    while (true) {
        AcceptRequest();
    }
}

void Server::Impl::AcceptRequest()
{
    worker_pool->SubmitTask(std::unique_ptr<Concurrent::Task>(new Request(acceptor.Accept(), dir)));
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
