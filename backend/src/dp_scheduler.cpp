// dp_scheduler.cpp — TRUE Dynamic Programming vs Greedy comparison
//
// DP formulation (Finite Horizon DP / Backward Induction):
//   State   : (time t,  q[0], q[1], q[2], q[3])  — time + all 4 queue sizes
//   Value   : V(state) = minimum total vehicle-seconds of wait from this state to end
//   Bellman : V(t,q) = min over (phase, duration):
//               { cost(yellow+green block) + V(t', q') }
//   Memoization: unordered_map caches every sub-problem so it is solved once.
//   Overlapping sub-problems: the SAME (t,q) state can be reached via many different
//   schedules; DP solves it once and reuses the answer → exponential saving over brute force.
//
// Queues capped at Q_CAP so state space stays finite and tractable.
// Compare against Greedy (no lookahead, no memoization) on a heavy-N scenario.
//
// Build(Win): g++ -std=c++17 -O2 -o dp.exe backend/src/dp_scheduler.cpp
// Build(Lin): g++ -std=c++17 -O2 -o dp     backend/src/dp_scheduler.cpp
// Run:        dp.exe

#include <algorithm>
#include <climits>
#include <cstdio>
#include <random>
#include <unordered_map>
#include <vector>

static const int T    = 60;   // planning horizon (smaller = faster DP)
static const int DIRS = 4;    // N S E W
static const int YLW  = 2;    // yellow clearance seconds
static const int MING = 5;    // minimum green seconds
static const int MAXG = 15;   // maximum green seconds (limited for tractability)
static const int SVC  = 2;    // cars served per green second
static const int QCAP = 20;   // cap queue values in DP state (keeps state space finite)

static int A[T][DIRS];                    // pre-generated arrivals[t][dir]
static const char* NM[] = {"N","S","E","W"};

// Pre-generate Poisson arrivals (same seed 42 as main server)
static void genArrivals(const int rates[]) {
    std::mt19937 rng(42);
    for (int t = 0; t < T; t++)
        for (int d = 0; d < DIRS; d++)
            A[t][d] = rates[d] > 0
                      ? std::poisson_distribution<int>(rates[d])(rng) : 0;
}

// ── DP state encoding ─────────────────────────────────────────────────────────
// Pack (t, q0, q1, q2, q3) into a single 64-bit key for the hash map.
// q values are capped at QCAP (≤20), so 5 bits each; t ≤ 63, so 6 bits.
// Total: 6 + 4*5 = 26 bits → fits in a uint64_t with no collisions.
static inline uint64_t encodeState(int t, const int q[]) {
    return ((uint64_t)t)
         | ((uint64_t)q[0] << 6)
         | ((uint64_t)q[1] << 11)
         | ((uint64_t)q[2] << 16)
         | ((uint64_t)q[3] << 21);
}

// Memoization table: state key → min future wait
static std::unordered_map<uint64_t, int> memo;
static int memoHits = 0;   // diagnostic: how many times a sub-problem was reused

// ── Forward simulation of one (yellow + green) block ─────────────────────────
// Returns {cost_incurred, end_time, new_q[4]} for a given action (phase, dur)
// starting from state (t, q[4]).  Does NOT recurse — pure forward sim.
struct Block { int cost, endT, q[DIRS]; };

static Block simBlock(int t, const int q[], int phase, int dur) {
    Block b; b.cost = 0; b.endT = t;
    for (int d = 0; d < DIRS; d++) b.q[d] = q[d];

    // Yellow clearance: arrivals only, no service
    for (int y = 0; y < YLW && b.endT < T; y++, b.endT++) {
        for (int d = 0; d < DIRS; d++) b.q[d] += A[b.endT][d];
        for (int d = 0; d < DIRS; d++) b.cost += b.q[d];
    }
    // Green: arrivals + service on active direction
    for (int g = 0; g < dur && b.endT < T; g++, b.endT++) {
        for (int d = 0; d < DIRS; d++) b.q[d] += A[b.endT][d];
        b.q[phase] = std::max(0, b.q[phase] - SVC);
        for (int d = 0; d < DIRS; d++) b.cost += b.q[d];
    }
    // Cap queues so memoization state space stays finite
    for (int d = 0; d < DIRS; d++) b.q[d] = std::min(b.q[d], QCAP);
    return b;
}

// ── TRUE DP: recursive with memoization ──────────────────────────────────────
// Returns: minimum total wait from (t, q[4]) to end of horizon.
// Overlapping sub-problems are detected and reused via `memo`.
static int dp(int t, const int q[]) {
    if (t >= T) return 0;   // base case: horizon reached, no more wait

    uint64_t key = encodeState(t, q);
    auto it = memo.find(key);
    if (it != memo.end()) { memoHits++; return it->second; }  // ← REUSE sub-problem

    int best = INT_MAX;
    // Try every (phase, duration) action
    for (int ph = 0; ph < DIRS; ph++)
        for (int dur = MING; dur <= MAXG && t + YLW + dur <= T; dur++) {
            Block b = simBlock(t, q, ph, dur);
            int future = dp(b.endT, b.q);           // ← RECURSIVE sub-problem
            if (future != INT_MAX && b.cost + future < best)
                best = b.cost + future;
        }

    memo[key] = best;    // ← STORE result so future calls reuse it
    return best;
}

// Reconstruct the DP schedule by tracing the optimal decisions
static std::vector<std::pair<int,int>> dpSchedule(const int initQ[]) {
    std::vector<std::pair<int,int>> sched;
    int t = 0, q[DIRS]; std::copy(initQ, initQ+DIRS, q);
    while (t < T) {
        int best = INT_MAX, bPh = 0, bDur = MING;
        for (int ph = 0; ph < DIRS; ph++)
            for (int dur = MING; dur <= MAXG && t + YLW + dur <= T; dur++) {
                Block b = simBlock(t, q, ph, dur);
                int future = dp(b.endT, b.q);
                if (future != INT_MAX && b.cost + future < best)
                { best = b.cost + future; bPh = ph; bDur = dur; }
            }
        Block b = simBlock(t, q, bPh, bDur);
        sched.push_back({bPh, bDur});
        std::copy(b.q, b.q+DIRS, q);
        t = b.endT;
    }
    return sched;
}

// ── Greedy scheduler (no lookahead, no memoization) ──────────────────────────
static std::vector<std::pair<int,int>> greedySchedule(const int initQ[]) {
    std::vector<std::pair<int,int>> sched;
    int t = 0, q[DIRS], rt[DIRS] = {};
    std::copy(initQ, initQ+DIRS, q);
    while (t < T) {
        // Score = queue + 0.5 * red_time (starvation prevention)
        float best = -1; int ph = 0;
        for (int d = 0; d < DIRS; d++) {
            float sc = q[d] + 0.5f * rt[d];
            if (sc > best) { best = sc; ph = d; }
        }
        int dur = std::min(std::max(MING, 5 + q[ph]/2), MAXG);
        if (t + YLW + dur > T) dur = std::max(1, T - t - YLW);
        Block b = simBlock(t, q, ph, dur);
        sched.push_back({ph, dur});
        std::copy(b.q, b.q+DIRS, q);
        for (int d = 0; d < DIRS; d++) rt[d] = (d==ph) ? 0 : rt[d]+dur+YLW;
        t = b.endT;
    }
    return sched;
}

// Compute total wait for a given schedule from initial state
static int totalWait(const std::vector<std::pair<int,int>>& sched, const int initQ[]) {
    int t = 0, q[DIRS], w = 0;
    std::copy(initQ, initQ+DIRS, q);
    for (size_t i = 0; i < sched.size(); i++) {
        Block b = simBlock(t, q, sched[i].first, sched[i].second);
        w += b.cost; std::copy(b.q, b.q+DIRS, q); t = b.endT;
    }
    return w;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    int rates[DIRS] = {3, 1, 1, 2};  // N=3 cars/s, W=2 cars/s — unequal traffic
    genArrivals(rates);

    int initQ[DIRS] = {5, 5, 5, 5};

    // 1. Run TRUE DP (builds memoization table bottom-up via recursion)
    int dpOpt = dp(0, initQ);
    auto dSch  = dpSchedule(initQ);
    int dWait  = totalWait(dSch, initQ);

    // 2. Run Greedy
    auto gSch  = greedySchedule(initQ);
    int gWait  = totalWait(gSch, initQ);

    printf("=== TRUE DP (memoized) vs Greedy ===\n");
    printf("Horizon: %ds  |  Arrivals N:%d S:%d E:%d W:%d  |  Qcap:%d\n\n",
           T, rates[0],rates[1],rates[2],rates[3], QCAP);

    printf("Greedy total wait    : %7d vehicle-seconds\n", gWait);
    printf("DP     total wait    : %7d vehicle-seconds\n", dWait);
    printf("DP optimal bound     : %7d vehicle-seconds (from dp(0,q))\n", dpOpt);
    double imp = gWait>0 ? 100.0*(gWait-dWait)/gWait : 0;
    printf("DP improvement       : %+.1f%%\n\n", imp);

    printf("Memo table size      : %d unique sub-problems stored\n", (int)memo.size());
    printf("Memo hits (reuses)   : %d  <- overlapping sub-problems reused\n\n", memoHits);

    printf("--- DP schedule ---\n");
    int t = 0;
    for (size_t i = 0; i < dSch.size(); i++) {
        int ph=dSch[i].first, d=dSch[i].second;
        printf("  t=%3d  yellow %ds -> %s green %ds\n", t, YLW, NM[ph], d);
        t += YLW + d;
    }
    printf("\n--- Greedy schedule ---\n");
    t = 0;
    for (size_t i = 0; i < gSch.size(); i++) {
        int ph=gSch[i].first, d=gSch[i].second;
        printf("  t=%3d  yellow %ds -> %s green %ds\n", t, YLW, NM[ph], d);
        t += YLW + d;
    }
    printf("\nDP sub-problem reuse demonstrates classical DP:\n");
    printf("  same (t, q0, q1, q2, q3) state reached via multiple schedule paths\n");
    printf("  is solved ONCE and cached — exponential saving over brute force.\n");
    return 0;
}
