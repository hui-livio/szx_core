#pragma once

#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace SZX::TEST::HTTP
{
// Deliberately independent of libhv: exercise the shipped DLL only through public headers and TCP.
class Client final
{
public:
#ifdef _WIN32
    using Handle                    = SOCKET;
    static constexpr Handle invalid = INVALID_SOCKET;
#else
    using Handle                    = int;
    static constexpr Handle invalid = -1;
#endif
    explicit Client(std::uint16_t port)
    {
#ifdef _WIN32
        static const auto initialized = []
        {
            WSADATA data{};
            return WSAStartup(MAKEWORD(2, 2), &data);
        }();
        if (initialized != 0)
            throw std::runtime_error("WSAStartup failed");
#endif
        fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd_ == invalid)
            throw std::runtime_error("socket failed");
#ifdef _WIN32
        const DWORD timeout = 3000;
#else
        const timeval timeout{3, 0};
#endif
        setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
        setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port   = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        if (connect(fd_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0)
        {
            close();
            throw std::runtime_error("connect failed");
        }
    }
    ~Client()
    {
        close();
    }
    Client(const Client &)                     = delete;
    auto operator=(const Client &) -> Client & = delete;
    void send(const std::string &data)
    {
        std::size_t offset = 0;
        while (offset < data.size())
        {
            const auto sent = ::send(fd_, data.data() + offset, static_cast<int>(data.size() - offset), 0);
            if (sent <= 0)
                throw std::runtime_error("send failed");
            offset += static_cast<std::size_t>(sent);
        }
    }
    auto read(bool head = false) -> std::string
    {
        char buffer[4096];
        for (;;)
        {
            const auto end = received_.find("\r\n\r\n");
            if (end != received_.npos)
            {
                std::size_t responseSize = end + 4;
                if (!head)
                {
                    const auto length = received_.find("Content-Length: ");
                    if (length == received_.npos || length >= end)
                        throw std::runtime_error("missing response length");
                    responseSize += std::stoull(received_.substr(length + 16));
                }
                if (received_.size() >= responseSize)
                {
                    auto response = received_.substr(0, responseSize);
                    received_.erase(0, responseSize);
                    return response;
                }
            }
            const auto n = recv(fd_, buffer, sizeof(buffer), 0);
            if (n == 0)
                throw std::runtime_error("connection closed before complete response");
            if (n < 0)
                throw std::runtime_error("receive timeout/error");
            received_.append(buffer, n);
        }
    }

    auto peerClosed() -> bool
    {
        char byte{};
        const auto result = recv(fd_, &byte, 1, 0);
#ifdef _WIN32
        return result == 0 || (result < 0 && WSAGetLastError() == WSAECONNRESET);
#else
        return result == 0 || (result < 0 && errno == ECONNRESET);
#endif
    }
    void close()
    {
        if (fd_ == invalid)
            return;
#ifdef _WIN32
        closesocket(fd_);
#else
        ::close(fd_);
#endif
        fd_ = invalid;
    }

private:
    Handle fd_ = invalid;
    std::string received_;
};

inline auto request(std::uint16_t port, const std::string &method, const std::string &path, const std::string &body = {},
                    const std::string &headers = {}) -> std::string
{
    Client client(port);
    client.send(method + " " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\n" + headers + "\r\n" + body);
    return client.read(method == "HEAD");
}
inline auto status(const std::string &response) -> int
{
    return response.size() >= 12 ? std::stoi(response.substr(9, 3)) : 0;
}
inline auto body(const std::string &response) -> std::string
{
    const auto position = response.find("\r\n\r\n");
    return position == response.npos ? std::string{} : response.substr(position + 4);
}
} // namespace SZX::TEST::HTTP
