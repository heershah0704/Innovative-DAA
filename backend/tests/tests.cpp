// tests.cpp
// Unit and scenario tests for the C++ logic. No framework: each test is a
// function, CHECK records failures, main() prints a summary.
//   build: g++ -std=c++17 -O2 -I src -o tests tests/tests.cpp   then run ./tests
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "arrivals.hpp"
#include "controllers.hpp"
#include "engine.hpp"
#include "metrics.hpp"
#include "priority_queue.hpp"
#include "scenarios.hpp"
#include "segment_tree.hpp"

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static Scenario preset(const std::string& id) { Scenario s; findScenario(id, s); return s; }

static Simulation makeSim(const std::string& strategy, const Scenario& s, const Timing& t = Timing{}) {
    return Simulation(s, t, makeController(strategy, t), generateArrivals(s));
}

static ControllerView viewOf(int qN, int qS, int qE, int qW, int greenElapsed = 20, PhaseId phase = NS) {
    ControllerView v;
    v.phase = phase;
    v.state = Light::Green;
    v.greenElapsed = greenElapsed;
    int q[4] = {qN, qS, qE, qW};
    for (int d = 0; d < 4; ++d) { v.a[d].queue = q[d]; v.a[d].arrivalRate = 0; v.a[d].serviceRate = 1; v.a[d].redTime = 0; }
    return v;
}

// ---------------------------------------------------------------- arrivals
void testArrivalsDeterministic() {
    Scenario a = preset("peak"), b = preset("peak");
    CHECK(generateArrivals(a) == generateArrivals(b));
    b.seed = 43;
    CHECK(generateArrivals(a) != generateArrivals(b));
}

void testIndependentStreams() {
    Scenario a = preset("peak"), b = preset("peak");
    b.arrivalRates[N] = 0.9;
    auto x = generateArrivals(a), y = generateArrivals(b);
    bool same = true;
    for (int t = 0; t < a.duration; ++t) for (int d : {S, E, W}) same = same && x[t][d] == y[t][d];
    CHECK(same);
}

void testZeroArrivals() {
    Scenario s = preset("peak");
    for (double& r : s.arrivalRates) r = 0;
    CHECK(totalArrivals(generateArrivals(s)) == 0);
}

// ---------------------------------------------------------------- simulation rules
void testInvariantsAllPresets() {
    for (const auto& sc : presetScenarios()) {
        for (std::string strategy : {"fixed", "greedy"}) {
            Simulation sim = makeSim(strategy, sc);
            bool ok = true;
            while (!sim.done()) {
                int before[4];
                for (int d = 0; d < 4; ++d) before[d] = sim.approach(d).queue + sim.schedule()[sim.time()][d];
                Signal sig = sim.signal();
                sim.step();
                for (int d = 0; d < 4; ++d) {
                    int q = sim.approach(d).queue;
                    int left = before[d] - q;
                    bool green = sig.state == Light::Green && phaseServes(sig.phase, d);
                    if (q < 0 || left < 0 || left > static_cast<int>(std::ceil(sim.approach(d).serviceRate))) ok = false;
                    if (!green && left != 0) ok = false;  // never served on red/yellow
                }
            }
            CHECK(ok);
            int start = 0;
            for (int d = 0; d < 4; ++d) start += sc.initialQueues[d];
            CHECK(start + sim.totalArrived() - sim.totalServed() == sim.totalQueue());  // cars conserved
        }
    }
}

void testTransitions() {
    for (std::string strategy : {"fixed", "greedy"}) {
        Simulation sim = makeSim(strategy, preset("single"));
        sim.runToEnd();
        const auto& f = sim.frames();
        bool ok = true;
        for (std::size_t i = 1; i < f.size(); ++i) {
            const Signal &p = f[i - 1].signal, &c = f[i].signal;
            if (p.state == Light::Green && c.state == Light::Green && p.phase != c.phase) ok = false;
            if (c.state == Light::Green && p.state != Light::Green && p.state != Light::AllRed) ok = false;
            if (c.state == Light::AllRed && p.state != Light::AllRed && p.state != Light::Yellow) ok = false;
        }
        CHECK(ok);
    }
}

void testGreenLimits() {
    Timing t;
    for (std::string strategy : {"fixed", "greedy"}) {
        Simulation sim = makeSim(strategy, preset("peak"), t);
        sim.runToEnd();
        for (const auto& d : sim.decisions())
            if (d.kind == "switch") CHECK(d.duration >= t.minGreen && d.duration <= t.maxGreen);
        int run = 0, longest = 0;
        for (const auto& f : sim.frames()) { run = f.signal.state == Light::Green ? run + 1 : 0; longest = std::max(longest, run); }
        CHECK(longest <= t.maxGreen + 1);
    }
}

void testNoStarvation() {
    Timing t;
    Scenario s = preset("single");
    s.initialQueues[E] = 300;
    s.arrivalRates[E] = 1.5;
    Simulation sim = makeSim("greedy", s, t);
    const int bound = t.maxGreen + 2 * t.lostTime();
    bool ok = true;
    while (sim.step()) for (int d = 0; d < 4; ++d) if (sim.approach(d).redTime > bound) ok = false;
    CHECK(ok);
}

void testEmptyIntersection() {
    Scenario s = preset("peak");
    for (int d = 0; d < 4; ++d) { s.arrivalRates[d] = 0; s.initialQueues[d] = 0; }
    for (std::string strategy : {"fixed", "greedy"}) {
        Simulation sim = makeSim(strategy, s);
        sim.runToEnd();
        Summary m = summarize(sim);
        CHECK(m.served == 0);
        CHECK(!m.hasAvgWait);  // shown as N/A
    }
}

void testOverload() {
    Scenario s = preset("peak");
    for (double& r : s.arrivalRates) r = 1.5;
    Simulation sim = makeSim("greedy", s);
    sim.runToEnd();
    CHECK(sim.totalQueue() > 167);
    CHECK(sim.totalServed() > 0);
}

void testShortRun() {
    Scenario s = preset("peak");
    s.duration = 3;
    Simulation sim = makeSim("greedy", s);
    sim.runToEnd();
    CHECK(sim.time() == 3);
    CHECK(sim.decisions().empty());
}

void testFairnessAndReproducible() {
    Scenario s = preset("peak");
    Schedule sched = generateArrivals(s);
    Timing t;
    Simulation a(s, t, makeController("fixed", t), sched), b(s, t, makeController("greedy", t), sched);
    a.runToEnd();
    b.runToEnd();
    CHECK(a.totalArrived() == b.totalArrived());
    Simulation b2 = makeSim("greedy", s);
    b2.runToEnd();
    CHECK(b2.totalWait() == b.totalWait() && b2.totalServed() == b.totalServed());
}

// ---------------------------------------------------------------- metrics (hand-checked)
void testMetricsByHand() {
    Scenario s = preset("peak");
    for (int d = 0; d < 4; ++d) { s.arrivalRates[d] = 0; s.initialQueues[d] = 0; s.serviceRates[d] = 1; }
    s.initialPhase = NS;
    s.initialGreenRemaining = 30;

    // North has 2 cars on green: 1 leaves, 1 waits 1 s, then it leaves. Delay 1, served 2, avg 0.5.
    s.initialQueues[N] = 2;
    s.duration = 2;
    Simulation a = makeSim("fixed", s);
    a.runToEnd();
    Summary m = summarize(a);
    CHECK(m.totalWait == 1 && m.served == 2 && m.avgWait == 0.5 && m.residual == 0);

    // East has 3 cars on red for 4 s: 12 car-seconds, nobody served.
    s.initialQueues[N] = 0;
    s.initialQueues[E] = 3;
    s.duration = 4;
    Simulation b = makeSim("fixed", s);
    b.runToEnd();
    m = summarize(b);
    CHECK(m.totalWait == 12 && m.served == 0 && !m.hasAvgWait && m.residual == 3);
}

void testFractionalService() {
    Scenario s = preset("peak");
    for (int d = 0; d < 4; ++d) { s.arrivalRates[d] = 0; s.initialQueues[d] = 0; }
    s.initialQueues[N] = 10;
    s.serviceRates[N] = 0.5;
    s.duration = 4;
    Simulation sim = makeSim("fixed", s);
    sim.runToEnd();
    CHECK(sim.approach(N).served == 2);
}

// ---------------------------------------------------------------- algorithms
struct IntBefore { bool operator()(int a, int b) const { return a > b; } };

void testHeapOrder() {
    MaxHeap<int, IntBefore> h;
    Mulberry32 rng(5);
    std::vector<int> xs;
    for (int i = 0; i < 200; ++i) { xs.push_back(static_cast<int>(rng.next() * 1000)); h.push(xs.back()); }
    std::sort(xs.begin(), xs.end(), std::greater<int>());
    std::vector<int> out;
    while (!h.empty()) out.push_back(h.pop());
    CHECK(out == xs);
}

void testPhaseTieBreak() {
    MaxHeap<RankedPhase, PhaseBefore> h;
    RankedPhase ew; ew.phase = EW; ew.priority = 10; ew.order = 1;
    RankedPhase ns; ns.phase = NS; ns.priority = 10; ns.order = 0;
    h.push(ew);
    h.push(ns);
    CHECK(h.pop().phase == NS);
}

void testRanking() {
    Timing t;
    ApproachScore sc[4];
    std::vector<std::string> skipped;
    ControllerView v = viewOf(2, 2, 40, 30);
    scoreApproaches(v, t, sc);
    CHECK(rankPhases(v, t, sc, skipped).top().phase == EW);

    v = viewOf(80, 80, 1, 0);
    v.a[E].redTime = v.a[W].redTime = t.maxWait;  // overdue
    scoreApproaches(v, t, sc);
    auto h = rankPhases(v, t, sc, skipped);
    CHECK(h.top().phase == EW && h.top().forced);

    v = viewOf(99, 99, 0, 0, t.maxGreen);
    scoreApproaches(v, t, sc);
    skipped.clear();
    auto h2 = rankPhases(v, t, sc, skipped);
    CHECK(h2.size() == 1 && h2.top().phase == EW && skipped.size() == 1);
}

void testAllocation() {
    Timing t;
    int cases[4][4] = {{0, 0, 0, 0}, {200, 200, 0, 0}, {200, 200, 200, 200}, {5, 0, 40, 3}};
    for (auto& q : cases) {
        Allocation a = allocateGreen(viewOf(q[0], q[1], q[2], q[3]), t);
        for (int p = 0; p < 2; ++p) CHECK(a.alloc[p] >= t.minGreen && a.alloc[p] <= t.maxGreen);
        CHECK(a.alloc[NS] + a.alloc[EW] <= t.cycleBudget);
    }
    Allocation busy = allocateGreen(viewOf(200, 200, 0, 0), t);
    CHECK(busy.alloc[NS] == t.maxGreen && busy.alloc[EW] == t.minGreen);
    Allocation ew = allocateGreen(viewOf(10, 5, 40, 35), t);
    CHECK(ew.alloc[EW] > ew.alloc[NS]);
}

void testValidation() {
    Timing t;
    CHECK(validateTiming(t).empty());
    t.minGreen = 50;
    auto e = validateTiming(t);
    bool mentionsBudget = false;
    for (auto& s : e) if (s.find("Cycle budget") != std::string::npos) mentionsBudget = true;
    CHECK(mentionsBudget);
    Timing bad;
    bad.yellow = -1;
    CHECK(!validateTiming(bad).empty());
}

void testSegmentTree() {
    Mulberry32 rng(9);
    const int n = 77;
    std::vector<int> v(n);
    auto sum = makeSumTree(n);
    auto mx = makeMaxTree(n);
    for (int i = 0; i < n; ++i) { v[i] = static_cast<int>(rng.next() * 50); sum.update(i, v[i]); mx.update(i, v[i]); }
    bool ok = true;
    for (int k = 0; k < 300; ++k) {
        if (k % 10 == 0) { int i = static_cast<int>(rng.next() * n); v[i] = static_cast<int>(rng.next() * 50); sum.update(i, v[i]); mx.update(i, v[i]); }
        int a = static_cast<int>(rng.next() * n);
        int b = a + static_cast<int>(rng.next() * (n - a));
        int s = 0, m = -1;
        for (int i = a; i <= b; ++i) { s += v[i]; m = std::max(m, v[i]); }
        if (sum.query(a, b) != s || mx.query(a, b) != m) ok = false;
    }
    CHECK(ok);
}

int main() {
    struct { const char* name; void (*fn)(); } tests[] = {
        {"arrivals are deterministic for a seed", testArrivalsDeterministic},
        {"each approach has its own random stream", testIndependentStreams},
        {"zero rates give zero arrivals", testZeroArrivals},
        {"queues never negative, service within limits, no service on red", testInvariantsAllPresets},
        {"switches pass through yellow then all-red", testTransitions},
        {"greens stay within min and max", testGreenLimits},
        {"every approach served within a bounded time", testNoStarvation},
        {"empty intersection: nothing served, average N/A", testEmptyIntersection},
        {"sustained overload stays valid", testOverload},
        {"run shorter than the opening green", testShortRun},
        {"baseline and greedy share arrivals; runs reproducible", testFairnessAndReproducible},
        {"metrics match hand calculations", testMetricsByHand},
        {"fractional service rates serve whole cars only", testFractionalService},
        {"heap pops in priority order", testHeapOrder},
        {"phase ties go to North-South", testPhaseTieBreak},
        {"ranking, overdue override, max-green exclusion", testRanking},
        {"greedy split respects min, max and budget", testAllocation},
        {"invalid timing is reported", testValidation},
        {"segment tree matches brute force", testSegmentTree},
    };
    for (auto& t : tests) {
        int before = failures;
        t.fn();
        std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", t.name);
    }
    std::printf("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
