// main.cpp
// Entry point. Starts the HTTP server, serves the frontend files and answers
// three API calls. All traffic logic lives in the .hpp files; this file only
// reads settings from the URL, runs the simulation and writes JSON.
//
//   GET /api/scenarios                 preset scenarios + default timing
//   GET /api/simulate?strategy=...&... full run: one frame per second + decisions + summary
//   GET /api/query?from=a&to=b&...     segment-tree lookup over seconds a..b of that run
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "arrivals.hpp"
#include "engine.hpp"
#include "http_server.hpp"
#include "json.hpp"
#include "metrics.hpp"
#include "scenarios.hpp"

using Query = std::map<std::string, std::string>;

// ------------------------------------------------------------ reading settings
struct Config {
    Scenario scenario;
    Timing timing;
    std::string strategy = "fixed";
};

static void readInt(const Query& q, const char* key, int& target, const char* label, std::vector<std::string>& errors) {
    auto it = q.find(key);
    if (it == q.end()) return;
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(it->second.c_str(), &end, 10);
    if (it->second.empty() || *end != '\0' || errno) errors.push_back(std::string(label) + " must be a whole number.");
    else target = static_cast<int>(v);
}

static void readDouble(const Query& q, const char* key, double& target, const char* label, std::vector<std::string>& errors) {
    auto it = q.find(key);
    if (it == q.end()) return;
    char* end = nullptr;
    double v = std::strtod(it->second.c_str(), &end);
    if (it->second.empty() || *end != '\0') errors.push_back(std::string(label) + " must be a number.");
    else target = v;
}

static std::vector<std::string> parseConfig(const Query& q, Config& c) {
    std::vector<std::string> errors;
    std::string id = q.count("scenario") ? q.at("scenario") : "peak";
    if (!findScenario(id, c.scenario)) return {"Unknown scenario: " + id};
    if (q.count("strategy")) c.strategy = q.at("strategy");
    if (c.strategy != "fixed" && c.strategy != "greedy") return {"Strategy must be fixed or greedy."};

    Scenario& s = c.scenario;
    Timing& t = c.timing;
    int seed = static_cast<int>(s.seed);
    readInt(q, "seed", seed, "Seed", errors);
    if (seed < 0) errors.push_back("Seed must be 0 or more.");
    s.seed = static_cast<uint32_t>(seed);
    readInt(q, "duration", s.duration, "Duration", errors);
    readInt(q, "green0", s.initialGreenRemaining, "Starting green time left", errors);
    if (q.count("phase")) {
        if (q.at("phase") == "NS") s.initialPhase = NS;
        else if (q.at("phase") == "EW") s.initialPhase = EW;
        else errors.push_back("Starting green must be NS or EW.");
    }
    for (int d = 0; d < 4; ++d) {
        std::string code = dirCode(d), name = dirName(d);
        readInt(q, ("q" + code).c_str(), s.initialQueues[d], (name + " starting queue").c_str(), errors);
        readDouble(q, ("r" + code).c_str(), s.arrivalRates[d], (name + " arrival rate").c_str(), errors);
        readDouble(q, ("s" + code).c_str(), s.serviceRates[d], (name + " service rate").c_str(), errors);
    }
    readInt(q, "minGreen", t.minGreen, "Minimum green", errors);
    readInt(q, "maxGreen", t.maxGreen, "Maximum green", errors);
    readInt(q, "yellow", t.yellow, "Yellow", errors);
    readInt(q, "allRed", t.allRed, "All-red", errors);
    readInt(q, "cycleBudget", t.cycleBudget, "Cycle budget", errors);
    readInt(q, "maxWait", t.maxWait, "Maximum wait", errors);
    readInt(q, "fixNS", t.fixedGreen[NS], "Fixed North-South green", errors);
    readInt(q, "fixEW", t.fixedGreen[EW], "Fixed East-West green", errors);
    readDouble(q, "alpha", t.alpha, "Weight alpha", errors);
    readDouble(q, "beta", t.beta, "Weight beta", errors);
    readDouble(q, "gamma", t.gamma, "Weight gamma", errors);
    if (!errors.empty()) return errors;

    errors = validateScenario(s, t);
    auto te = validateTiming(t);
    errors.insert(errors.end(), te.begin(), te.end());
    return errors;
}

// ------------------------------------------------------------ writing JSON
static void writeScenario(JsonWriter& j, const Scenario& s) {
    j.beginObject();
    j.key("id").value(s.id).key("name").value(s.name).key("note").value(s.note);
    j.key("seed").value(s.seed).key("duration").value(s.duration);
    j.key("initialPhase").value(phaseCode(s.initialPhase)).key("initialGreenRemaining").value(s.initialGreenRemaining);
    const char* groups[3] = {"initialQueues", "arrivalRates", "serviceRates"};
    for (int g = 0; g < 3; ++g) {
        j.key(groups[g]).beginObject();
        for (int d = 0; d < 4; ++d) {
            j.key(dirCode(d));
            if (g == 0) j.value(s.initialQueues[d]);
            else j.value(g == 1 ? s.arrivalRates[d] : s.serviceRates[d]);
        }
        j.endObject();
    }
    j.endObject();
}

static void writeTiming(JsonWriter& j, const Timing& t) {
    j.beginObject();
    j.key("minGreen").value(t.minGreen).key("maxGreen").value(t.maxGreen);
    j.key("yellow").value(t.yellow).key("allRed").value(t.allRed);
    j.key("cycleBudget").value(t.cycleBudget).key("maxWait").value(t.maxWait);
    j.key("fixedGreen").beginObject().key("NS").value(t.fixedGreen[NS]).key("EW").value(t.fixedGreen[EW]).endObject();
    j.key("weights").beginObject().key("alpha").value(t.alpha).key("beta").value(t.beta).key("gamma").value(t.gamma).endObject();
    j.endObject();
}

static void writeSignal(JsonWriter& j, const Signal& s) {
    j.beginObject();
    j.key("phase").value(phaseCode(s.phase)).key("state").value(lightName(s.state));
    j.key("remaining").value(s.remaining).key("greenElapsed").value(s.greenElapsed).key("greenPlanned").value(s.greenPlanned);
    j.key("next");
    if (s.hasNext) j.beginObject().key("phase").value(phaseCode(s.nextPhase)).key("duration").value(s.nextDuration).endObject();
    else j.null();
    j.endObject();
}

static void writeDecision(JsonWriter& j, const Decision& d) {
    j.beginObject();
    j.key("time").value(d.time).key("from").value(phaseCode(d.from)).key("phase").value(phaseCode(d.phase));
    j.key("duration").value(d.duration).key("kind").value(d.kind).key("strategy").value(d.strategy);
    j.key("explanation").value(d.explanation);
    j.key("constraints").beginArray();
    for (const auto& c : d.constraints) j.value(c);
    j.endArray();
    j.key("scores");
    if (d.hasScores) {
        j.beginObject();
        for (int k = 0; k < 4; ++k) {
            const auto& s = d.scores[k];
            j.key(dirCode(k)).beginObject().key("queue").value(s.queue).key("predicted").value(s.predicted)
             .key("wait").value(s.wait).key("score").value(s.score).endObject();
        }
        j.endObject();
    } else j.null();
    j.key("ranking").beginArray();
    for (const auto& r : d.ranking)
        j.beginObject().key("phase").value(phaseCode(r.phase)).key("score").value(r.score).key("forced").value(r.forced).endObject();
    j.endArray();
    j.key("alloc");
    if (d.hasAlloc) j.beginObject().key("NS").value(d.alloc[NS]).key("EW").value(d.alloc[EW]).endObject();
    else j.null();
    j.key("overridden").value(d.overridden);
    j.endObject();
}

static void writeSummary(JsonWriter& j, const Summary& s) {
    j.beginObject();
    j.key("served").value(s.served).key("arrived").value(s.arrived).key("totalWait").value(s.totalWait);
    j.key("avgWait");
    if (s.hasAvgWait) j.value(s.avgWait); else j.null();
    j.key("residual").value(s.residual).key("maxQueue").value(s.maxQueue).key("maxTotalQueue").value(s.maxTotalQueue);
    j.key("switches").value(s.switches);
    j.endObject();
}

static void writeFrame(JsonWriter& j, const Frame& f) {
    j.beginObject();
    j.key("t").value(f.t).key("total").value(f.total).key("served").value(f.served).key("wait").value(f.wait).key("switches").value(f.switches);
    auto arr = [&](const char* k, const int* v) { j.key(k).beginArray(); for (int d = 0; d < 4; ++d) j.value(v[d]); j.endArray(); };
    arr("q", f.q);
    arr("red", f.red);
    arr("maxQ", f.maxQ);
    j.key("signal");
    writeSignal(j, f.signal);
    j.endObject();
}

static HttpResponse errorResponse(const std::vector<std::string>& errors) {
    JsonWriter j;
    j.beginObject().key("ok").value(false).key("errors").beginArray();
    for (const auto& e : errors) j.value(e);
    j.endArray().endObject();
    // 200, not 400: invalid settings are a normal answer ("these values are not allowed"), not a crash.
    return {200, "application/json", j.str()};
}

// ------------------------------------------------------------ API handlers
static HttpResponse handleScenarios() {
    JsonWriter j;
    j.beginObject().key("scenarios").beginArray();
    for (const auto& s : presetScenarios()) writeScenario(j, s);
    j.endArray().key("timing");
    writeTiming(j, Timing{});
    j.endObject();
    return {200, "application/json", j.str()};
}

static HttpResponse handleSimulate(const Query& q) {
    Config c;
    auto errors = parseConfig(q, c);
    if (!errors.empty()) return errorResponse(errors);

    Schedule schedule = generateArrivals(c.scenario);
    Simulation sim(c.scenario, c.timing, makeController(c.strategy, c.timing), schedule);
    sim.runToEnd();

    JsonWriter j;
    j.beginObject();
    j.key("ok").value(true);
    j.key("strategy").value(c.strategy);
    j.key("controller").value(sim.controllerName());
    j.key("scheduleId").value(fingerprint(schedule));
    j.key("duration").value(c.scenario.duration);
    j.key("frames").beginArray();
    for (const auto& f : sim.frames()) writeFrame(j, f);
    j.endArray();
    j.key("decisions").beginArray();
    for (const auto& d : sim.decisions()) writeDecision(j, d);
    j.endArray();
    j.key("summary");
    writeSummary(j, summarize(sim));
    j.endObject();
    return {200, "application/json", j.str()};
}

static HttpResponse handleQuery(const Query& q) {
    Config c;
    auto errors = parseConfig(q, c);
    if (!errors.empty()) return errorResponse(errors);
    int from = -1, to = -1;
    readInt(q, "from", from, "From", errors);
    readInt(q, "to", to, "To", errors);
    if (errors.empty() && (from < 0 || to > c.scenario.duration || from >= to))
        errors.push_back("Pick whole seconds with 0 <= from < to <= " + std::to_string(c.scenario.duration) + ".");
    if (!errors.empty()) return errorResponse(errors);

    Simulation sim(c.scenario, c.timing, makeController(c.strategy, c.timing), generateArrivals(c.scenario));
    while (sim.time() < to) sim.step();

    JsonWriter j;
    j.beginObject().key("ok").value(true).key("from").value(from).key("to").value(to);
    j.key("arrivals").value(sim.arrivalsTree().query(from, to - 1));  // arrivals during seconds from..to-1
    j.key("peakQueue").value(sim.queueTree().query(from, to));        // queue at the end of each second
    j.endObject();
    return {200, "application/json", j.str()};
}

// ------------------------------------------------------------ static files
static std::string frontendDir;

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static std::string mimeType(const std::string& path) {
    auto ends = [&](const char* ext) { std::string e = ext; return path.size() >= e.size() && path.compare(path.size() - e.size(), e.size(), e) == 0; };
    if (ends(".html")) return "text/html; charset=utf-8";
    if (ends(".css")) return "text/css; charset=utf-8";
    if (ends(".js")) return "text/javascript; charset=utf-8";
    if (ends(".svg")) return "image/svg+xml";
    return "application/octet-stream";
}

static HttpResponse handleStatic(const std::string& path) {
    std::string rel = path == "/" ? "/index.html" : path;
    if (rel.find("..") != std::string::npos) return {404, "text/plain", "Not found"};
    std::string body;
    if (!readFile(frontendDir + rel, body)) return {404, "text/plain", "Not found: " + rel};
    return {200, mimeType(rel), body};
}

int main(int argc, char** argv) {
    int port = argc > 1 ? std::atoi(argv[1]) : 8080;
    if (argc > 2) frontendDir = argv[2];
    else {
        for (const char* guess : {"frontend", "../frontend", "../../frontend"}) {
            std::string tmp;
            if (readFile(std::string(guess) + "/index.html", tmp)) { frontendDir = guess; break; }
        }
    }
    if (frontendDir.empty()) {
        std::fprintf(stderr, "Could not find the frontend folder. Run from the project folder, or pass it: server 8080 path/to/frontend\n");
        return 1;
    }

    return runServer(port, [](const HttpRequest& req) -> HttpResponse {
        if (req.method != "GET") return {400, "text/plain", "Only GET is supported"};
        if (req.path == "/api/scenarios") return handleScenarios();
        if (req.path == "/api/simulate") return handleSimulate(req.query);
        if (req.path == "/api/query") return handleQuery(req.query);
        return handleStatic(req.path);
    });
}
