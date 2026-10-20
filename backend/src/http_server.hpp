// http_server.hpp
// A very small HTTP/1.1 server on plain sockets (works on Windows and Linux/macOS).
// It handles one request at a time, which is plenty for one person using the app.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <map>
#include <string>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socket_t = SOCKET;
  inline void closeSocket(socket_t s) { closesocket(s); }
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  using socket_t = int;
  inline void closeSocket(socket_t s) { close(s); }
  #define INVALID_SOCKET (-1)
#endif

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json";
    std::string body;
};

inline std::string urlDecode(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i + 2 < s.size()) {
            char hex[3] = {s[i + 1], s[i + 2], 0};
            char* end = nullptr;
            long v = std::strtol(hex, &end, 16);
            if (*end != '\0') { out += s[i]; continue; }  // not valid %XX: keep as-is
            out += static_cast<char>(v);
            i += 2;
        } else out += s[i];
    }
    return out;
}

inline HttpRequest parseRequestLine(const std::string& raw) {
    HttpRequest req;
    std::size_t sp1 = raw.find(' ');
    std::size_t sp2 = raw.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return req;
    req.method = raw.substr(0, sp1);
    std::string target = raw.substr(sp1 + 1, sp2 - sp1 - 1);
    std::size_t qm = target.find('?');
    req.path = urlDecode(target.substr(0, qm));
    if (qm != std::string::npos) {
        std::string qs = target.substr(qm + 1);
        std::size_t start = 0;
        while (start <= qs.size()) {
            std::size_t amp = qs.find('&', start);
            std::string pair = qs.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
            std::size_t eq = pair.find('=');
            if (!pair.empty()) {
                if (eq == std::string::npos) req.query[urlDecode(pair)] = "";
                else req.query[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
            }
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }
    return req;
}

inline const char* statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        default: return "Internal Server Error";
    }
}

inline int runServer(int port, const std::function<HttpResponse(const HttpRequest&)>& handler) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { std::fprintf(stderr, "WSAStartup failed\n"); return 1; }
#endif
    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) { std::fprintf(stderr, "Could not create socket\n"); return 1; }
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // listen on all interfaces (required for Render/cloud)
    addr.sin_port = htons(static_cast<unsigned short>(port));
    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        std::fprintf(stderr, "Port %d is busy. Try: server %d\n", port, port + 1);
        return 1;
    }
    listen(server, 16);
    std::printf("Traffic signal simulator running at http://localhost:%d  (Ctrl+C to stop)\n", port);
    std::fflush(stdout);

    while (true) {
        socket_t client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        std::string raw;
        char buf[4096];
        while (raw.find("\r\n\r\n") == std::string::npos && raw.size() < 65536) {
            int n = recv(client, buf, sizeof buf, 0);
            if (n <= 0) break;
            raw.append(buf, n);
        }

        HttpResponse res;
        HttpRequest req = parseRequestLine(raw);
        if (req.method.empty()) res = {400, "text/plain", "Bad request"};
        else {
            try { res = handler(req); }
            catch (const std::exception& e) { res = {500, "text/plain", std::string("Server error: ") + e.what()}; }
        }

        std::string head = "HTTP/1.1 " + std::to_string(res.status) + " " + statusText(res.status) +
                           "\r\nContent-Type: " + res.contentType +
                           "\r\nContent-Length: " + std::to_string(res.body.size()) +
                           "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
        std::string all = head + res.body;
        std::size_t sent = 0;
        while (sent < all.size()) {
            int n = send(client, all.data() + sent, static_cast<int>(all.size() - sent), 0);
            if (n <= 0) break;
            sent += n;
        }
        closeSocket(client);
    }
}
