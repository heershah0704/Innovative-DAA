// Traffic Signal Optimizer — greedy priority-queue controller
// Directions: N=0 S=1 E=2 W=3.  Two phases: 0=NS green, 1=EW green.
// Greedy rule: when green expires, pick the phase with MORE waiting cars.
// Green duration: proportional to winning queue, clamped to [MIN_GREEN, MAX_GREEN].
//
// Build (Windows): g++ -std=c++17 -O2 -o server.exe backend/src/main.cpp -lws2_32
// Build (Linux):   g++ -std=c++17 -O2 -o server   backend/src/main.cpp
#include <algorithm>
#include <cstdio>
#include <cstdlib>
// <cstring> removed — not used
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
static const int SERVICE   = 2;   // cars cleared per green direction per second
static const int MIN_GREEN = 5;   // minimum green phase duration (seconds)
static const int MAX_GREEN = 30;  // maximum green phase duration (seconds)

struct Sim {
    int  q[4]   = {5,5,5,5};   // waiting-car queues  N S E W
    int  arr[4] = {1,1,1,1};   // arrival rates  (cars / second)
    int  phase  = 0;            // current green phase: 0=NS  1=EW
    int  timer  = 10;           // seconds remaining on this green
    int  t      = 0;            // simulation clock
    std::mt19937 rng{42};

    // Advance one second of simulation
    void step() {
        // FIX: guard arr[i]==0 — poisson_distribution requires mean > 0
        for (int i = 0; i < 4; i++) {
            if (arr[i] > 0) {
                std::poisson_distribution<int> pd(arr[i]);
                q[i] += pd(rng);
            }
        }
        // Serve the two green directions (N+S or E+W)
        int a = (phase == 0) ? 0 : 2;   // index of first green direction
        q[a]   = std::max(0, q[a]   - SERVICE);
        q[a+1] = std::max(0, q[a+1] - SERVICE);
        t++;
        if (--timer <= 0) switchPhase();
    }

    int redTime[2] = {};  // seconds each phase has been red (starvation fix)

    // Greedy decision using a priority queue (max-heap):
    // Priority = waiting cars + 0.5 * seconds_on_red  (prevents starvation).
    // Green time proportional to winner's queue, clamped to [MIN_GREEN, MAX_GREEN].
    void switchPhase() {
        int ns = q[0] + q[1];
        int ew = q[2] + q[3];
        // FIX starvation: weight red-time into priority so neither side waits forever
        float scoreNS = ns + 0.5f * redTime[0];
        float scoreEW = ew + 0.5f * redTime[1];
        phase = (scoreEW > scoreNS) ? 1 : 0;
        redTime[phase] = 0;          // winner resets its red counter
        redTime[1-phase]++;          // loser accumulates more red time
        int total = (phase == 0) ? ns : ew;
        int raw = 5 + total / 2;
        timer = raw < MIN_GREEN ? MIN_GREEN : raw > MAX_GREEN ? MAX_GREEN : raw;
    }

    // Serialize state to JSON
    std::string json() const {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"q\":[%d,%d,%d,%d],\"arr\":[%d,%d,%d,%d],"
            "\"phase\":%d,\"timer\":%d,\"t\":%d}",
            q[0],q[1],q[2],q[3],
            arr[0],arr[1],arr[2],arr[3],
            phase, timer, t);
        return buf;
    }

    void reset() { *this = Sim{}; }
} sim;

// ── HTTP helpers ──────────────────────────────────────────────────────────────
// FIX qparam: only match key at start of string or after '&' to avoid
// matching "n=" inside "dn=" etc.
static std::string qparam(const std::string& qs, const char* key) {
    std::string k = std::string(key) + "=";
    std::size_t pos = 0;
    while (pos <= qs.size()) {
        auto found = qs.find(k, pos);
        if (found == std::string::npos) return "";
        if (found == 0 || qs[found-1] == '&') {   // must be at boundary
            pos = found + k.size();
            auto end = qs.find('&', pos);
            return qs.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        }
        pos = found + 1;
    }
    return "";
}

static bool readFile(const std::string& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf(); out = ss.str(); return true;
}

static std::string mime(const std::string& p) {
    if (p.size() > 5 && p.compare(p.size()-5,5,".html")==0) return "text/html;charset=utf-8";
    if (p.size() > 3 && p.compare(p.size()-3,3,".js")==0)   return "text/javascript";
    if (p.size() > 4 && p.compare(p.size()-4,4,".css")==0)  return "text/css";
    return "text/plain";
}

// ── Request handler ───────────────────────────────────────────────────────────
static std::string handle(const char* raw) {
    std::string line(raw);
    line = line.substr(0, line.find("\r\n"));
    auto sp1 = line.find(' '), sp2 = line.find(' ', sp1+1);
    std::string tgt = (sp1 == std::string::npos) ? "/" : line.substr(sp1+1, sp2-sp1-1);
    auto qm = tgt.find('?');
    std::string path = tgt.substr(0, qm);
    std::string qs   = (qm == std::string::npos) ? "" : tgt.substr(qm+1);

    std::string body, ct = "application/json";
    int status = 200;

    if (path == "/api/state") {
        body = sim.json();
    } else if (path == "/api/step") {
        // FIX: cap n so a huge value can't hang the server
        int n = qs.empty() ? 1 : std::max(1, std::atoi(qparam(qs,"n").c_str()));
        if (n > 10000) n = 10000;
        for (int i = 0; i < n; i++) sim.step();
        body = sim.json();
    } else if (path == "/api/reset") {
        sim.reset();
        body = sim.json();
    } else if (path == "/api/set") {
        // ?dir=0&arr=2   — set arrival rate for direction
        // ?dir=0&add=10  — manually add cars to a direction's queue
        int dir = std::atoi(qparam(qs,"dir").c_str());
        if (dir >= 0 && dir < 4) {
            auto a   = qparam(qs,"arr");
            auto add = qparam(qs,"add");
            if (!a.empty()) { int v=std::atoi(a.c_str()); sim.arr[dir]=v<0?0:v>5?5:v; }
            if (!add.empty()) sim.q[dir] = std::max(0, sim.q[dir] + std::atoi(add.c_str()));
        }
        body = sim.json();
    } else {
        std::string rel = (path == "/" || path.empty()) ? "/index.html" : path;
        if (rel.find("..") != std::string::npos) {
            status = 403; body = "Forbidden"; ct = "text/plain";
        } else if (!readFile("frontend" + rel, body)) {
            status = 404; body = "Not found"; ct = "text/plain";
        } else {
            ct = mime(rel);
        }
    }

    // FIX: correct status text for non-200 responses
    const char* txt = (status==200)?"OK":(status==403)?"Forbidden":"Not Found";
    return "HTTP/1.1 " + std::to_string(status) + " " + txt + "\r\n"
           "Content-Type: "   + ct   + "\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Cache-Control: no-store\r\n"
           "Connection: close\r\n\r\n" + body;
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    netInit();
    int port = 8080;
    if (const char* ep = std::getenv("PORT")) port = std::atoi(ep);
    else if (argc > 1) port = std::atoi(argv[1]);

    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((unsigned short)port);
    if (bind(srv, (sockaddr*)&addr, sizeof addr) != 0) {
        fputs("Port busy\n", stderr); return 1;
    }
    listen(srv, 8);
    printf("Running at http://localhost:%d\n", port);
    fflush(stdout);

    for (;;) {
        sock_t cl = accept(srv, nullptr, nullptr);
        if (cl == INVALID_SOCKET) continue;
        char buf[4096] = {};
        int n = (int)recv(cl, buf, sizeof buf - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            auto resp = handle(buf);
            send(cl, resp.c_str(), (int)resp.size(), 0);
        }
        CLOSE(cl);
    }
}
