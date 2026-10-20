// scenarios.hpp
// The two preset traffic situations and the rules for valid settings.
#pragma once
#include <cmath>
#include <string>
#include <vector>
#include "model.hpp"

inline std::vector<Scenario> presetScenarios() {
    Scenario peak;
    peak.id = "peak";
    peak.name = "Peak demo (default)";
    peak.note = "The demo queues from the brief. North is badly backed up.";
    peak.seed = 42;
    peak.duration = 180;
    int pq[4] = {82, 21, 51, 13};
    double pr[4] = {0.35, 0.15, 0.25, 0.10};
    for (int d = 0; d < 4; ++d) { peak.initialQueues[d] = pq[d]; peak.arrivalRates[d] = pr[d]; peak.serviceRates[d] = 1.0; }

    Scenario single;
    single.id = "single";
    single.name = "Single-approach congestion";
    single.note = "East alone carries more than a fixed 30 s green can clear.";
    single.seed = 23;
    single.duration = 180;
    int sq[4] = {5, 5, 40, 5};
    double sr[4] = {0.08, 0.08, 0.55, 0.08};
    for (int d = 0; d < 4; ++d) { single.initialQueues[d] = sq[d]; single.arrivalRates[d] = sr[d]; single.serviceRates[d] = 1.0; }

    return {peak, single};
}

inline bool findScenario(const std::string& id, Scenario& out) {
    for (const auto& s : presetScenarios()) {
        if (s.id == id) { out = s; return true; }
    }
    return false;
}

inline std::vector<std::string> validateTiming(const Timing& t) {
    std::vector<std::string> e;
    auto range = [&](int v, const char* label, int lo, int hi) {
        if (v < lo || v > hi) e.push_back(std::string(label) + " must be a whole number from " + std::to_string(lo) + " to " + std::to_string(hi) + ".");
    };
    range(t.minGreen, "Minimum green", 1, 120);
    range(t.maxGreen, "Maximum green", 1, 300);
    range(t.yellow, "Yellow", 1, 10);
    range(t.allRed, "All-red", 0, 10);
    range(t.cycleBudget, "Cycle budget", 2, 600);
    range(t.maxWait, "Maximum wait", 1, 1000);
    if (!e.empty()) return e;

    if (t.maxGreen < t.minGreen) e.push_back("Maximum green cannot be shorter than minimum green.");
    if (t.cycleBudget < t.minGreen * PHASE_COUNT)
        e.push_back("Cycle budget (" + std::to_string(t.cycleBudget) + " s) is less than the two minimum greens it has to cover (2 x " + std::to_string(t.minGreen) + " s).");
    if (t.maxWait < t.minGreen + t.lostTime())
        e.push_back("Maximum wait must be at least minimum green plus yellow and all-red (" + std::to_string(t.minGreen + t.lostTime()) + " s).");
    for (int p = 0; p < PHASE_COUNT; ++p) {
        if (t.fixedGreen[p] < t.minGreen || t.fixedGreen[p] > t.maxGreen)
            e.push_back(std::string("Fixed-time green for ") + phaseLabel(PhaseId(p)) + " must be between " + std::to_string(t.minGreen) + " and " + std::to_string(t.maxGreen) + " s.");
    }
    const double w[3] = {t.alpha, t.beta, t.gamma};
    const char* names[3] = {"alpha", "beta", "gamma"};
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(w[i]) || w[i] < 0 || w[i] > 100) e.push_back(std::string("Weight ") + names[i] + " must be a number from 0 to 100.");
    return e;
}

inline std::vector<std::string> validateScenario(const Scenario& s, const Timing& t) {
    std::vector<std::string> e;
    if (s.duration < 1 || s.duration > 3600) e.push_back("Duration must be a whole number of seconds from 1 to 3600.");
    for (int d = 0; d < 4; ++d) {
        std::string n = dirName(d);
        if (s.initialQueues[d] < 0 || s.initialQueues[d] > 999) e.push_back(n + " starting queue must be a whole number from 0 to 999.");
        if (!std::isfinite(s.arrivalRates[d]) || s.arrivalRates[d] < 0 || s.arrivalRates[d] > 3) e.push_back(n + " arrival rate must be between 0 and 3 vehicles/s.");
        if (!std::isfinite(s.serviceRates[d]) || s.serviceRates[d] <= 0 || s.serviceRates[d] > 5) e.push_back(n + " service rate must be above 0 and at most 5 vehicles/s.");
    }
    if (s.initialGreenRemaining < 1 || s.initialGreenRemaining > t.maxGreen)
        e.push_back("Starting green time left must be a whole number from 1 to " + std::to_string(t.maxGreen) + " s.");
    return e;
}
