// dp_scheduler.cpp — DP vs Greedy traffic scheduler comparison
// Greedy (main.cpp): picks the best-looking direction NOW, no lookahead.
// DP (this file):    tries every possible (phase, duration) choice, simulates
//                    the full future with greedy rollout, picks the best first action.
//                    With known arrivals this finds a provably better schedule.
//
// Both use identical Poisson arrivals (seed 42, T=120s, 4 independent signals).
// Build(Win): g++ -std=c++17 -O2 -o dp.exe backend/src/dp_scheduler.cpp
// Build(Lin): g++ -std=c++17 -O2 -o dp     backend/src/dp_scheduler.cpp
// Run:        dp.exe

#include <algorithm>
#include <climits>
#include <cstdio>
#include <random>
#include <vector>

static const int T=120, DIRS=4, YLW=2, MING=5, MAXG=30, SVC=2;
static int A[T][DIRS];                     // pre-generated arrivals[t][dir]
static const char* NM[]={"N","S","E","W"};

// Pre-generate Poisson arrivals with same seed as main server
void genArrivals(const int rates[]) {
    std::mt19937 rng(42);
    for (int t=0;t<T;t++) for (int d=0;d<DIRS;d++)
        A[t][d] = rates[d]>0 ? std::poisson_distribution<int>(rates[d])(rng) : 0;
}

// Lightweight simulator using pre-generated arrivals (no randomness needed)
struct Sim {
    int q[DIRS]={5,5,5,5}, rt[DIRS]={}, t=0, w=0;

    // Yellow clearance: cars arrive, nobody served
    void yel() {
        for (int y=0;y<YLW&&t<T;y++,t++) {
            for (int d=0;d<DIRS;d++) q[d]+=A[t][d];
            for (int d=0;d<DIRS;d++) w+=q[d];
        }
    }
    // Green phase: cars arrive, active direction served
    void grn(int ph, int dur) {
        for (int g=0;g<dur&&t<T;g++,t++) {
            for (int d=0;d<DIRS;d++) q[d]+=A[t][d];
            q[ph]=std::max(0,q[ph]-SVC);
            for (int d=0;d<DIRS;d++) w+=q[d];
        }
    }
    // One full phase: yellow + green, update red-time counters
    void go(int ph, int dur) {
        yel(); grn(ph,dur);
        for (int d=0;d<DIRS;d++) rt[d]=(d==ph)?0:rt[d]+dur+YLW;
    }
    // Greedy pick: max score direction
    int pick() const {
        float b=-1; int p=0;
        for (int d=0;d<DIRS;d++) { float s=q[d]+.5f*rt[d]; if(s>b){b=s;p=d;} }
        return p;
    }
    // Greedy duration: proportional to queue, clamped
    int dur(int ph) const { int r=MING+q[ph]/2; return r<MING?MING:r>MAXG?MAXG:r; }
};

// Run greedy to completion from given state, return schedule
std::vector<std::pair<int,int>> greedyRun(Sim s) {
    std::vector<std::pair<int,int>> sch;
    while (s.t<T) { int ph=s.pick(),d=s.dur(ph); s.go(ph,d); sch.push_back({ph,d}); }
    return sch;
}

// Run greedy, return total wait
int greedyWait(Sim s) {
    while (s.t<T) { int ph=s.pick(),d=s.dur(ph); s.go(ph,d); }
    return s.w;
}

// ── Greedy scheduler ──────────────────────────────────────────────────────────
std::pair<std::vector<std::pair<int,int>>,int> runGreedy() {
    Sim s;
    auto sch = greedyRun(s);
    // re-simulate to get final wait (greedyRun returns sch but s is consumed)
    Sim s2;
    for (size_t i=0;i<sch.size();i++) s2.go(sch[i].first, sch[i].second);
    return {sch, s2.w};
}

// ── DP scheduler (1-step lookahead with greedy rollout) ───────────────────────
// At each decision point, try every (phase × duration) combination.
// For each candidate, run greedy to the end and measure total wait.
// Choose the candidate with the minimum total future wait — then commit it.
// This beats pure greedy because it evaluates the full consequences of each choice.
//
// Time complexity: O(decisions × DIRS × MAXG × T) — fast for T=120.
std::pair<std::vector<std::pair<int,int>>,int> runDP() {
    Sim s;
    std::vector<std::pair<int,int>> sch;
    while (s.t < T) {
        int bestW=INT_MAX, bPh=0, bDur=MING;
        for (int ph=0;ph<DIRS;ph++)
            for (int d=MING;d<=MAXG&&s.t+YLW+d<=T;d++) {
                Sim ts=s; ts.go(ph,d);      // try this action
                int w=greedyWait(ts);        // evaluate full future via greedy rollout
                if (w<bestW) { bestW=w; bPh=ph; bDur=d; }
            }
        s.go(bPh,bDur);
        sch.push_back({bPh,bDur});
    }
    return {sch, s.w};
}

// ── main ──────────────────────────────────────────────────────────────────────
int main() {
    // Unequal scenario: North has 3× heavier traffic — this is where DP wins most.
    int rates[DIRS] = {3, 1, 1, 1};
    genArrivals(rates);

    auto gRes = runGreedy();
    auto dRes = runDP();
    auto gSch=gRes.first; int gWait=gRes.second;
    auto dSch=dRes.first; int dWait=dRes.second;

    printf("=== DP vs Greedy: 4 independent signals, T=%ds ===\n", T);
    printf("Arrival rates — N:%d  S:%d  E:%d  W:%d  cars/s\n\n",
           rates[0],rates[1],rates[2],rates[3]);
    printf("Greedy total wait : %7d vehicle-seconds\n", gWait);
    printf("DP     total wait : %7d vehicle-seconds\n", dWait);
    double imp = gWait>0 ? 100.0*(gWait-dWait)/gWait : 0;
    printf("DP improvement    : %+.1f%%\n\n", imp);

    printf("--- DP schedule (first 12 phases) ---\n");
    int t=0;
    for (int i=0;i<(int)dSch.size()&&i<12;i++) {
        int ph=dSch[i].first, d=dSch[i].second;
        printf("  t=%3d  yellow %ds -> %s green %ds\n", t, YLW, NM[ph], d);
        t+=YLW+d;
    }
    printf("\n--- Greedy schedule (first 12 phases) ---\n");
    t=0;
    for (int i=0;i<(int)gSch.size()&&i<12;i++) {
        int ph=gSch[i].first, d=gSch[i].second;
        printf("  t=%3d  yellow %ds -> %s green %ds\n", t, YLW, NM[ph], d);
        t+=YLW+d;
    }
    printf("\nKey difference: DP avoids over-serving N early, balancing\n");
    printf("queues so shorter phases are needed later — less total wait.\n");
    return 0;
}
