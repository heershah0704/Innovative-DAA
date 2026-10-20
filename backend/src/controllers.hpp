// controllers.hpp
// The two signal controllers. Each one is asked a single question every time a
// green ends: "which phase goes next, and for how many seconds?"
//
//   FixedTimeController - the baseline. Alternates phases with fixed greens.
//   GreedyController    - the optimizer. A priority queue picks the phase,
//                         a greedy split of the cycle picks the green length.
#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "model.hpp"
#include "priority_queue.hpp"

inline std::string fmt1(double x) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", x);
    return b;
}

class Controller {
public:
    virtual ~Controller() = default;
    virtual std::string name() const = 0;
    virtual Decision decide(const ControllerView& view) = 0;
};

// ---------------------------------------------------------------- baseline
class FixedTimeController : public Controller {
public:
    explicit FixedTimeController(const Timing& t) : t_(t) {}
    std::string name() const override { return "Fixed-time"; }

    Decision decide(const ControllerView& view) override {
        Decision d;
        d.strategy = "Fixed-time";
        d.kind = "switch";
        d.phase = otherPhase(view.phase);
        d.duration = t_.fixedGreen[d.phase];
        d.hasAlloc = true;
        d.alloc[NS] = t_.fixedGreen[NS];
        d.alloc[EW] = t_.fixedGreen[EW];
        d.constraints = {
            "Greens are fixed at " + std::to_string(t_.fixedGreen[NS]) + " s (North-South) and " + std::to_string(t_.fixedGreen[EW]) + " s (East-West).",
            "Every switch goes through " + std::to_string(t_.yellow) + " s yellow and " + std::to_string(t_.allRed) + " s all-red.",
        };
        d.explanation = std::string(phaseLabel(d.phase)) + " gets its fixed " + std::to_string(d.duration) + " s. Queue lengths are not looked at.";
        return d;
    }

private:
    Timing t_;
};

// ---------------------------------------------------------------- greedy + PQ

// Length of one full cycle: the green budget plus each phase's yellow + all-red.
inline int cycleEstimate(const Timing& t) { return t.cycleBudget + PHASE_COUNT * t.lostTime(); }

// Step 1: score each approach.
//   score = alpha * cars queued + beta * cars expected in one cycle + gamma * seconds on red
inline void scoreApproaches(const ControllerView& v, const Timing& t, ApproachScore out[4]) {
    const int horizon = cycleEstimate(t);
    for (int d = 0; d < 4; ++d) {
        out[d].queue = v.a[d].queue;
        out[d].predicted = v.a[d].arrivalRate * horizon;
        out[d].wait = v.a[d].redTime;
        out[d].score = t.alpha * out[d].queue + t.beta * out[d].predicted + t.gamma * out[d].wait;
    }
}

// Heap order: higher priority first; on a tie the phase listed first (North-South) wins.
struct PhaseBefore {
    bool operator()(const RankedPhase& a, const RankedPhase& b) const {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.order < b.order;
    }
};
constexpr double FORCE_BONUS = 1e6;

// Step 2: put the phases in a max-heap by priority (sum of their approaches' scores).
// An approach red for maxWait seconds or more makes its phase "overdue" (huge bonus).
// A phase that has already been green for maxGreen is not allowed back in.
inline MaxHeap<RankedPhase, PhaseBefore> rankPhases(const ControllerView& v, const Timing& t,
                                                    const ApproachScore scores[4],
                                                    std::vector<std::string>& skipped) {
    MaxHeap<RankedPhase, PhaseBefore> heap;
    for (int p = 0; p < PHASE_COUNT; ++p) {
        PhaseId id = PhaseId(p);
        bool isCurrent = v.state == Light::Green && v.phase == id;
        if (isCurrent && v.greenElapsed >= t.maxGreen) {
            skipped.push_back(std::string(phaseLabel(id)) + " has been green for the " + std::to_string(t.maxGreen) + " s maximum, so it cannot be extended.");
            continue;
        }
        RankedPhase r;
        r.phase = id;
        r.order = p;
        for (Dir d : phaseApproaches(id)) {
            r.score += scores[d].score;
            if (!isCurrent && v.a[d].redTime >= t.maxWait) r.overdue.push_back(d);
        }
        r.forced = !r.overdue.empty();
        r.priority = r.forced ? FORCE_BONUS + r.score : r.score;
        heap.push(r);
    }
    return heap;
}

// Step 3: greedy split of the cycle's green budget, one second at a time.
// Both phases start at minGreen. Each extra second goes to the phase that would
// let the most still-waiting cars through in that second. Stops at maxGreen,
// when the budget is used up, or early when no phase would gain anything.
struct Allocation {
    bool feasible = true;
    int alloc[2] = {0, 0};
    int budget = 0;
    int used = 0;
    std::vector<std::string> constraints;
    std::string error;
};

inline Allocation allocateGreen(const ControllerView& v, const Timing& t) {
    Allocation a;
    const int horizon = cycleEstimate(t);
    double demand[4];
    for (int d = 0; d < 4; ++d) demand[d] = t.alpha * v.a[d].queue + t.beta * v.a[d].arrivalRate * horizon;

    a.alloc[NS] = a.alloc[EW] = t.minGreen;
    a.budget = t.cycleBudget - t.minGreen * PHASE_COUNT;
    a.constraints.push_back("Each phase starts at the " + std::to_string(t.minGreen) + " s minimum green.");
    if (a.budget < 0) {
        a.feasible = false;
        a.error = "Cycle budget cannot cover two minimum greens.";
        return a;
    }

    auto gain = [&](PhaseId p) {
        double g = 0;
        for (Dir d : phaseApproaches(p)) {
            double mu = v.a[d].serviceRate;
            double stillWaiting = std::max(0.0, demand[d] - mu * a.alloc[p]);
            g += std::min(mu, stillWaiting);
        }
        return g;
    };

    while (a.used < a.budget) {
        int best = -1;
        double bestGain = 0;
        for (int p = 0; p < PHASE_COUNT; ++p) {
            if (a.alloc[p] >= t.maxGreen) continue;
            double g = gain(PhaseId(p));
            if (g > bestGain + 1e-12) { best = p; bestGain = g; }
        }
        if (best < 0) break;
        ++a.alloc[best];
        ++a.used;
    }

    for (int p = 0; p < PHASE_COUNT; ++p)
        if (a.alloc[p] >= t.maxGreen) a.constraints.push_back(std::string(phaseLabel(PhaseId(p))) + " is capped at the " + std::to_string(t.maxGreen) + " s maximum green.");
    a.constraints.push_back("Used " + std::to_string(a.used) + " of the " + std::to_string(a.budget) + " s left in the " + std::to_string(t.cycleBudget) + " s cycle budget.");
    if (a.used < a.budget && (a.alloc[NS] < t.maxGreen || a.alloc[EW] < t.maxGreen))
        a.constraints.push_back("The other " + std::to_string(a.budget - a.used) + " s were left unused: no queue needed them.");
    return a;
}

class GreedyController : public Controller {
public:
    explicit GreedyController(const Timing& t) : t_(t) {}
    std::string name() const override { return "Greedy + priority queue"; }

    Decision decide(const ControllerView& v) override {
        Decision d;
        d.strategy = name();
        d.hasScores = true;
        scoreApproaches(v, t_, d.scores);

        std::vector<std::string> skipped;
        auto heap = rankPhases(v, t_, d.scores, skipped);
        d.ranking = heap.sorted();

        Allocation al = allocateGreen(v, t_);
        d.hasAlloc = true;
        d.alloc[NS] = al.alloc[NS];
        d.alloc[EW] = al.alloc[EW];
        d.constraints = al.constraints;
        d.constraints.insert(d.constraints.end(), skipped.begin(), skipped.end());

        // Pop the most urgent phase. If it is the phase already green, keep it
        // only if its greedy share still has time left; otherwise pop the next one.
        RankedPhase top = heap.pop();
        bool chosen = false;
        if (top.phase == v.phase && v.state == Light::Green) {
            int ext = std::min(al.alloc[v.phase] - v.greenElapsed, t_.maxGreen - v.greenElapsed);
            if (ext >= 1) {
                d.phase = v.phase;
                d.duration = ext;
                d.kind = "extend";
                chosen = true;
            } else {
                d.constraints.push_back(std::string(phaseLabel(v.phase)) + " has already had " + std::to_string(v.greenElapsed) + " s, at least its " + std::to_string(al.alloc[v.phase]) + " s share, so it was not extended.");
                top = heap.pop();
            }
        }
        if (!chosen) {
            d.phase = top.phase;
            d.duration = std::max(t_.minGreen, std::min(al.alloc[d.phase], t_.maxGreen));
            d.kind = d.phase == v.phase ? "extend" : "switch";
        }

        // Plain-language reason.
        std::string ranks;
        for (std::size_t i = 0; i < d.ranking.size(); ++i) {
            if (i) ranks += " vs ";
            ranks += std::string(phaseLabel(d.ranking[i].phase)) + " " + fmt1(d.ranking[i].score) + (d.ranking[i].forced ? " (overdue)" : "");
        }
        const RankedPhase* picked = nullptr;
        for (const auto& r : d.ranking) if (r.phase == d.phase) picked = &r;

        std::string why;
        if (picked && picked->forced) {
            std::string who;
            for (std::size_t i = 0; i < picked->overdue.size(); ++i) who += (i ? " and " : "") + std::string(dirName(picked->overdue[i]));
            why = who + " had been red for " + std::to_string(t_.maxWait) + " s or more, so " + phaseLabel(d.phase) + " was forced to the front of the queue.";
        } else if (d.ranking.front().phase == d.phase) {
            why = std::string(phaseLabel(d.phase)) + " has the highest priority (" + ranks + ").";
        } else {
            why = std::string(phaseLabel(d.ranking.front().phase)) + " ranked first but had used its share, so the next phase in the queue, " + phaseLabel(d.phase) + ", goes.";
        }
        std::string action = d.kind == "extend"
            ? "Extend " + std::string(phaseLabel(d.phase)) + " by " + std::to_string(d.duration) + " s."
            : "Switch to " + std::string(phaseLabel(d.phase)) + " for " + std::to_string(d.duration) + " s.";
        d.explanation = action + " " + why + " Priority per approach = " + fmt1(t_.alpha) + " x queue + " + fmt1(t_.beta) +
                        " x expected arrivals + " + fmt1(t_.gamma) + " x seconds on red. The green length comes from the greedy split of the cycle: each second above the minimum went to the phase that would clear the most waiting cars with it.";
        return d;
    }

private:
    Timing t_;
};
