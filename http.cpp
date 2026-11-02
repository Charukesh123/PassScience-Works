#include "http.hpp"
#include "../core/formats.hpp"
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
static void closeSock(sock_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
static const sock_t INVALID_SOCKET = -1;
static void closeSock(sock_t s) { close(s); }
#endif
static sock_t g_listen = INVALID_SOCKET;

static std::string urldecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) { o += (char)std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
        else if (s[i] == '+') o += ' '; else o += s[i];
    }
    return o;
}
int httpListen(int first) {
#ifdef _WIN32
    WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w)) return 0;
#endif
    g_listen = socket(AF_INET, SOCK_STREAM, 0); if (g_listen == INVALID_SOCKET) return 0;
    for (int p = first; p <= first + 20; ++p) {
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons((unsigned short)p); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(g_listen, (sockaddr*)&a, sizeof a) == 0 && listen(g_listen, 16) == 0) return p;
    }
    return 0;
}
static void send_all(sock_t s, const std::string& d) { size_t o = 0; while (o < d.size()) { int n = (int)send(s, d.data() + o, (int)std::min<size_t>(d.size() - o, 1 << 20), 0); if (n <= 0) return; o += n; } }
static void reply(sock_t s, const Response& r) {
    const char* st = r.status == 200 ? "OK" : r.status == 400 ? "Bad Request" : r.status == 404 ? "Not Found" : r.status == 413 ? "Payload Too Large" : "Error";
    std::string h = "HTTP/1.1 " + std::to_string(r.status) + " " + st + "\r\nContent-Type: " + r.type + "; charset=utf-8\r\nContent-Length: " + std::to_string(r.body.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    send_all(s, h); send_all(s, r.body);
}
static void handle(sock_t s, const Handler& h) {
    std::string buf; char tmp[65536]; size_t hdrEnd = std::string::npos;
    while (hdrEnd == std::string::npos && buf.size() < 65536) { int n = (int)recv(s, tmp, sizeof tmp, 0); if (n <= 0) { closeSock(s); return; } buf.append(tmp, n); hdrEnd = buf.find("\r\n\r\n"); }
    if (hdrEnd == std::string::npos) { closeSock(s); return; }
    Request rq; std::string head = buf.substr(0, hdrEnd); size_t le = head.find("\r\n");
    std::string line = head.substr(0, le); size_t a = line.find(' '), b = line.find(' ', a + 1);
    if (a == std::string::npos || b == std::string::npos) { closeSock(s); return; }
    rq.method = line.substr(0, a); std::string target = line.substr(a + 1, b - a - 1);
    size_t q = target.find('?'); rq.path = target.substr(0, q);
    if (q != std::string::npos) { std::string qs = target.substr(q + 1); size_t p = 0; while (p <= qs.size()) { size_t e = qs.find('&', p); if (e == std::string::npos) e = qs.size(); std::string kv = qs.substr(p, e - p); size_t eq = kv.find('='); if (!kv.empty()) rq.query[urldecode(kv.substr(0, eq))] = eq == std::string::npos ? "" : urldecode(kv.substr(eq + 1)); p = e + 1; } }
    size_t clen = 0; { std::string lh = head; for (auto& c : lh) c = (char)std::tolower((unsigned char)c); size_t p = lh.find("content-length:"); if (p != std::string::npos) clen = (size_t)std::strtoull(lh.c_str() + p + 15, nullptr, 10); }
    if (clen > wb::kMaxInputBytes + 1024) { Response r; r.status = 413; r.body = "{\"error\":\"File exceeds the 100 MB limit of this prototype.\"}"; reply(s, r); closeSock(s); return; }
    rq.body = buf.substr(hdrEnd + 4);
    while (rq.body.size() < clen) { int n = (int)recv(s, tmp, sizeof tmp, 0); if (n <= 0) break; rq.body.append(tmp, n); }
    Response r; try { r = h(rq); } catch (const std::exception& e) { r.status = 400; r.body = std::string("{\"error\":\"") + e.what() + "\"}"; }
    reply(s, r); closeSock(s);
}
void httpServe(Handler h) {
    for (;;) { sock_t c = accept(g_listen, nullptr, nullptr); if (c == INVALID_SOCKET) continue; std::thread([c, h] { handle(c, h); }).detach(); }
}
