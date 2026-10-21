// Traffic Signal Optimizer — 4 independent signals, greedy + priority queue
// Real-life model: N, S, E, W each have their OWN dedicated green phase.
// Yellow clearance interval between every phase change (safety requirement).
// Fixes from PDF: yellow/all-red, starvation, Poisson(0), missing dir,
//                 unlimited n, status text, qparam boundary, recv loop, INADDR.
// Build(Win): g++ -std=c++17 -O2 -o server.exe backend/src/main.cpp -lws2_32
// Build(Lin): g++ -std=c++17 -O2 -o server   backend/src/main.cpp
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
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

// ── Simulation ────────────────────────────────────────────────────────────────
// 4 independent phases: 0=N  1=S  2=E  3=W (one direction exclusively green).
// FIX no-yellow: real clearance interval between every phase change.
static const int SERVICE    = 2;  // cars cleared per second when green
static const int MIN_GREEN  = 5;  // minimum green duration (s)
static const int MAX_GREEN  = 30; // maximum green duration (s)
static const int YELLOW_DUR = 2;  // FIX: clearance seconds between phases

struct Sim {
    int  q[4]       = {5,5,5,5};  // queues  N S E W
    int  arr[4]     = {1,1,1,1};  // arrival rates  cars/s
    int  phase      = 0;           // active direction: 0=N 1=S 2=E 3=W
    bool yellow     = false;       // FIX: true during yellow clearance interval
    int  timer      = 10;          // seconds left in current state
    int  t          = 0;
    int  redTime[4] = {};          // starvation counter per direction
    std::mt19937 rng{42};

    void step() {
        // FIX Poisson(0): guard — poisson_distribution requires mean > 0
        for (int i = 0; i < 4; i++)
            if (arr[i] > 0) { std::poisson_distribution<int> pd(arr[i]); q[i] += pd(rng); }

        if (!yellow)
            q[phase] = std::max(0, q[phase] - SERVICE); // serve only during green
        // Track red time for starvation prevention
        for (int i = 0; i < 4; i++)
            if (i != phase || yellow) redTime[i]++;
        t++;
        if (--timer <= 0) {
            if (!yellow) { yellow = true;  timer = YELLOW_DUR; } // green → yellow
            else         { yellow = false; switchPhase();        } // yellow → next green
        }
    }

    // Greedy priority-queue: score = waiting cars + 0.5×redTime (starvation fix)
    // With 4 directions, this is a max-heap of 4 items.
    void switchPhase() {
        float best = -1; int next = 0;
        for (int i = 0; i < 4; i++) {
            float sc = q[i] + 0.5f * redTime[i];
            if (sc > best) { best = sc; next = i; }
        }
        redTime[next] = 0;  // winner resets its red counter
        phase = next;
        int raw = 5 + q[phase] / 2;
        timer = raw < MIN_GREEN ? MIN_GREEN : raw > MAX_GREEN ? MAX_GREEN : raw;
    }

    std::string json() const {
        char buf[300];
        snprintf(buf, sizeof(buf),
            "{\"q\":[%d,%d,%d,%d],\"arr\":[%d,%d,%d,%d],"
            "\"phase\":%d,\"yellow\":%s,\"timer\":%d,\"t\":%d}",
            q[0],q[1],q[2],q[3], arr[0],arr[1],arr[2],arr[3],
            phase, yellow ? "true" : "false", timer, t);
        return buf;
    }
    void reset() { *this = Sim{}; }
} sim;

// ── HTTP helpers ──────────────────────────────────────────────────────────────
// FIX qparam boundary: only match key at start or after '&' (not inside another key)
static std::string qparam(const std::string& qs, const char* key) {
    std::string k = std::string(key) + "=";
    std::size_t pos = 0;
    while (pos <= qs.size()) {
        auto f = qs.find(k, pos);
        if (f == std::string::npos) return "";
        if (f == 0 || qs[f-1] == '&') {
            f += k.size(); auto e = qs.find('&', f);
            return qs.substr(f, e == std::string::npos ? std::string::npos : e - f);
        }
        pos = f + 1;
    }
    return "";
}

static bool readFile(const std::string& p, std::string& out) {
    std::ifstream f(p, std::ios::binary); if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf(); out = ss.str(); return true;
}

static std::string mime(const std::string& p) {
    if (p.size()>5 && p.compare(p.size()-5,5,".html")==0) return "text/html;charset=utf-8";
    if (p.size()>3 && p.compare(p.size()-3,3,".js")==0)   return "text/javascript";
    if (p.size()>4 && p.compare(p.size()-4,4,".css")==0)  return "text/css";
    return "text/plain";
}

// ── Request handler ───────────────────────────────────────────────────────────
static std::string handle(const char* raw) {
    std::string line(raw);
    line = line.substr(0, line.find("\r\n"));
    auto sp1 = line.find(' '), sp2 = line.find(' ', sp1+1);
    std::string tgt = (sp1==std::string::npos) ? "/" : line.substr(sp1+1, sp2-sp1-1);
    auto qm = tgt.find('?');
    std::string path = tgt.substr(0, qm), qs = (qm==std::string::npos) ? "" : tgt.substr(qm+1);
    std::string body, ct = "application/json"; int status = 200;

    if (path == "/api/state") {
        body = sim.json();
    } else if (path == "/api/step") {
        // FIX unlimited n: cap at 10000
        int n = qs.empty() ? 1 : std::max(1, std::atoi(qparam(qs,"n").c_str()));
        if (n > 10000) n = 10000;
        for (int i = 0; i < n; i++) sim.step();
        body = sim.json();
    } else if (path == "/api/reset") {
        sim.reset(); body = sim.json();
    } else if (path == "/api/set") {
        // FIX missing dir: require explicit dir parameter (atoi("")=0 defaults to North)
        auto dstr = qparam(qs, "dir");
        if (dstr.empty()) { body = "{\"error\":\"dir parameter required\"}"; }
        else {
            int dir = std::atoi(dstr.c_str());
            if (dir >= 0 && dir < 4) {
                auto a = qparam(qs,"arr"), add = qparam(qs,"add");
                if (!a.empty())   { int v=std::atoi(a.c_str()); sim.arr[dir]=v<0?0:v>5?5:v; }
                if (!add.empty()) sim.q[dir] = std::max(0, sim.q[dir]+std::atoi(add.c_str()));
            }
            body = sim.json();
        }
    } else {
        std::string rel = (path=="/"||path.empty()) ? "/index.html" : path;
        if (rel.find("..") != std::string::npos) { status=403; body="Forbidden"; ct="text/plain"; }
        else if (!readFile("frontend"+rel, body))  { status=404; body="Not found"; ct="text/plain"; }
        else ct = mime(rel);
    }

    // FIX status text: correct HTTP reason phrase for each code
    const char* txt = (status==200)?"OK":(status==403)?"Forbidden":"Not Found";
    return "HTTP/1.1 " + std::to_string(status) + " " + txt + "\r\n"
           "Content-Type: " + ct + "\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Cache-Control: no-store\r\nConnection: close\r\n\r\n" + body;
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    netInit();
    int port = 8080;
    if (const char* ep = std::getenv("PORT")) port = std::atoi(ep);
    else if (argc > 1) port = std::atoi(argv[1]);

    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in addr{}; addr.sin_family = AF_INET;
    // FIX INADDR note: INADDR_ANY = cloud/LAN accessible. Use INADDR_LOOPBACK for laptop-only.
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(srv,(sockaddr*)&addr,sizeof addr)) { fputs("Port busy\n",stderr); return 1; }
    listen(srv, 8);
    printf("Running at http://localhost:%d\n", port); fflush(stdout);

    for (;;) {
        sock_t cl = accept(srv, nullptr, nullptr);
        if (cl == INVALID_SOCKET) continue;
        // FIX one-read: loop recv until full HTTP header arrives (handles large/split requests)
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
