// Traffic Signal Optimizer — 4 independent signals, DYNAMIC PROGRAMMING
// Same model/API as main.cpp (greedy) but switchPhase() uses DP:
//   State : (k, q[N], q[S], q[E], q[W])   k = how many future phases already planned
//   V(k,q) = min over (phase p, green d) [ cost(p,d,q) + V(k+1, q') ],  V(H,q) = 0
//   cost   = sum of all waiting cars every second (= total vehicle-seconds of delay)
//   Memo   : unordered_map<state,value>  → overlapping sub-problems solved once
// Build(Win): g++ -std=c++17 -O2 -o dp_server.exe backend/src/dp_server.cpp -lws2_32
// Run     : dp_server.exe 8081
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#ifdef _WIN32
  #include <winsock2.h>
  #pragma comment(lib,"ws2_32.lib")
  using sock_t = SOCKET;
  #define CLOSE(s) closesocket(s)
  inline void netInit(){WSADATA w;WSAStartup(MAKEWORD(2,2),&w);}
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  using sock_t = int;
  #define CLOSE(s) close(s)
  #define INVALID_SOCKET -1
  inline void netInit(){}
#endif

static const int SERVICE = 2, MIN_GREEN = 5, MAX_GREEN = 30, YELLOW_DUR = 2;
static const int HORIZON = 3;                       // plan 3 phases ahead
static const int DURS[] = {5, 10, 15, 20, 25, 30};  // candidate green lengths
static const int MAX_RED = 60;                      // fairness guard (no starvation)

struct Sim {
    int  q[4] = {5,5,5,5}, arr[4] = {1,1,1,1};
    int  phase = 0, timer = 10, t = 0, redTime[4] = {};
    bool yellow = false;
    long memoSize = 0, memoHits = 0, lastCost = 0;   // DP diagnostics for UI
    std::mt19937 rng{42};
    std::unordered_map<unsigned long long, long> memo;

    void step() {
        for (int i = 0; i < 4; i++)
            if (arr[i] > 0) { std::poisson_distribution<int> pd(arr[i]); q[i] += pd(rng); }
        if (!yellow) q[phase] = std::max(0, q[phase] - SERVICE);
        for (int i = 0; i < 4; i++) if (i != phase || yellow) redTime[i]++;
        t++;
        if (--timer <= 0) {
            if (!yellow) { yellow = true; timer = YELLOW_DUR; }
            else         { yellow = false; switchPhase(); }
        }
    }

    // Simulate one block (green d for phase p, then yellow) with expected arrivals.
    long block(int p, int d, int* nq) const {
        long cost = 0;
        for (int s = 0; s < d + YELLOW_DUR; s++)
            for (int i = 0; i < 4; i++) {
                nq[i] += arr[i];
                if (i == p && s < d) nq[i] = std::max(0, nq[i] - SERVICE);
                nq[i] = std::min(nq[i], 1023);       // cap so state packs in 10 bits
                cost += nq[i];
            }
        return cost;
    }

    // Bellman recursion with memoization (top-down DP).
    long V(int k, const int* cq) {
        if (k == HORIZON) return 0;                  // base case
        unsigned long long key = k;
        for (int i = 0; i < 4; i++) key = (key << 10) | (unsigned)cq[i];
        auto it = memo.find(key);
        if (it != memo.end()) { memoHits++; return it->second; }
        long best = -1;
        for (int p = 0; p < 4; p++)
            for (int d : DURS) {
                int nq[4] = {cq[0], cq[1], cq[2], cq[3]};
                long c = block(p, d, nq) + V(k + 1, nq);
                if (best < 0 || c < best) best = c;
            }
        return memo[key] = best;
    }

    // DP decision: pick (phase, duration) minimising total future delay.
    void switchPhase() {
        memo.clear(); memoHits = 0;
        int bp = 0, bd = MIN_GREEN; long best = -1;
        int starving = -1;
        for (int i = 0; i < 4; i++) if (redTime[i] >= MAX_RED && q[i] > 0) starving = i;
        for (int p = 0; p < 4; p++) {
            if (starving >= 0 && p != starving) continue;
            for (int d : DURS) {
                int nq[4] = {std::min(q[0],1023), std::min(q[1],1023), std::min(q[2],1023), std::min(q[3],1023)};
                long c = block(p, d, nq) + V(1, nq);
                if (best < 0 || c < best) { best = c; bp = p; bd = d; }
            }
        }
        memoSize = (long)memo.size(); lastCost = best;
        phase = bp; redTime[bp] = 0;
        timer = std::max(MIN_GREEN, std::min(MAX_GREEN, bd));
    }

    std::string json() const {
        char buf[400];
        snprintf(buf, sizeof(buf),
            "{\"q\":[%d,%d,%d,%d],\"arr\":[%d,%d,%d,%d],\"phase\":%d,\"yellow\":%s,"
            "\"timer\":%d,\"t\":%d,\"memo\":%ld,\"hits\":%ld,\"cost\":%ld}",
            q[0],q[1],q[2],q[3], arr[0],arr[1],arr[2],arr[3], phase,
            yellow ? "true" : "false", timer, t, memoSize, memoHits, lastCost);
        return buf;
    }
    void reset() { *this = Sim{}; }
} sim;

// ── HTTP (same as main.cpp) ───────────────────────────────────────────────────
static std::string qparam(const std::string& qs, const char* key) {
    std::string k = std::string(key) + "="; std::size_t pos = 0;
    while (pos <= qs.size()) {
        auto f = qs.find(k, pos); if (f == std::string::npos) return "";
        if (f == 0 || qs[f-1] == '&') {
            f += k.size(); auto e = qs.find('&', f);
            return qs.substr(f, e == std::string::npos ? std::string::npos : e - f);
        }
        pos = f + 1;
    }
    return "";
}

static std::string handle(const char* raw) {
    std::string line(raw); line = line.substr(0, line.find("\r\n"));
    auto sp1 = line.find(' '), sp2 = line.find(' ', sp1+1);
    std::string tgt = (sp1==std::string::npos) ? "/" : line.substr(sp1+1, sp2-sp1-1);
    auto qm = tgt.find('?');
    std::string path = tgt.substr(0, qm), qs = (qm==std::string::npos) ? "" : tgt.substr(qm+1);
    std::string body; int status = 200;

    if (path == "/api/state") body = sim.json();
    else if (path == "/api/step") {
        int n = qs.empty() ? 1 : std::max(1, std::atoi(qparam(qs,"n").c_str()));
        if (n > 10000) n = 10000;
        for (int i = 0; i < n; i++) sim.step();
        body = sim.json();
    } else if (path == "/api/reset") { sim.reset(); body = sim.json(); }
    else if (path == "/api/set") {
        auto dstr = qparam(qs, "dir");
        if (dstr.empty()) body = "{\"error\":\"dir parameter required\"}";
        else {
            int dir = std::atoi(dstr.c_str());
            if (dir >= 0 && dir < 4) {
                auto a = qparam(qs,"arr"), add = qparam(qs,"add");
                if (!a.empty())   { int v=std::atoi(a.c_str()); sim.arr[dir]=v<0?0:v>5?5:v; }
                if (!add.empty()) sim.q[dir] = std::max(0, sim.q[dir]+std::atoi(add.c_str()));
            }
            body = sim.json();
        }
    } else { status = 404; body = "{\"error\":\"not found\"}"; }

    return "HTTP/1.1 " + std::to_string(status) + (status==200 ? " OK" : " Not Found") + "\r\n"
           "Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
           "\r\nAccess-Control-Allow-Origin: *\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + body;
}

int main(int argc, char** argv) {
    netInit();
    int port = argc > 1 ? std::atoi(argv[1]) : 8081;
    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in addr{}; addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(srv,(sockaddr*)&addr,sizeof addr)) { fputs("Port busy\n",stderr); return 1; }
    listen(srv, 8);
    printf("DP server at http://localhost:%d\n", port); fflush(stdout);
    for (;;) {
        sock_t cl = accept(srv, nullptr, nullptr);
        if (cl == INVALID_SOCKET) continue;
        char buf[8192] = {}; int total = 0;
        while (total < (int)sizeof(buf)-1) {
            int n = (int)recv(cl, buf+total, sizeof(buf)-1-total, 0);
            if (n <= 0) break; total += n;
            if (std::string(buf, total).find("\r\n\r\n") != std::string::npos) break;
        }
        if (total > 0) { auto r = handle(buf); send(cl, r.c_str(), (int)r.size(), 0); }
        CLOSE(cl);
    }
}
