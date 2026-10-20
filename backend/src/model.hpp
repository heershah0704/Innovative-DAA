// model.hpp
// The basic data types shared by every other file.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// The four approaches (roads coming into the junction).
enum Dir { N = 0, S = 1, E = 2, W = 3 };
constexpr int DIR_COUNT = 4;
inline const char* dirCode(int d) { static const char* c[] = {"N", "S", "E", "W"}; return c[d]; }
inline const char* dirName(int d) { static const char* c[] = {"North", "South", "East", "West"}; return c[d]; }

// Two phases. North+South move together; East+West move together.
enum PhaseId { NS = 0, EW = 1 };
constexpr int PHASE_COUNT = 2;
inline const char* phaseCode(PhaseId p) { return p == NS ? "NS" : "EW"; }
inline const char* phaseLabel(PhaseId p) { return p == NS ? "North-South" : "East-West"; }
inline PhaseId otherPhase(PhaseId p) { return p == NS ? EW : NS; }
inline std::array<Dir, 2> phaseApproaches(PhaseId p) {
    return p == NS ? std::array<Dir, 2>{N, S} : std::array<Dir, 2>{E, W};
}
inline bool phaseServes(PhaseId p, int d) {
    return p == NS ? (d == N || d == S) : (d == E || d == W);
}

// The light that the active phase is showing.
enum class Light { Green, Yellow, AllRed };
inline const char* lightName(Light l) {
    return l == Light::Green ? "green" : l == Light::Yellow ? "yellow" : "allRed";
}

// Signal timing rules (all in whole seconds except the weights).
struct Timing {
    int minGreen = 10;
    int maxGreen = 60;
    int yellow = 3;
    int allRed = 2;
    int cycleBudget = 90;       // green seconds shared by both phases in one cycle
    int maxWait = 60;           // red this long -> forced to the front of the queue
    int fixedGreen[2] = {30, 30};  // fixed-time baseline greens for NS and EW
    double alpha = 1.0;         // weight on cars already queued
    double beta = 1.0;          // weight on cars expected during one cycle
    double gamma = 0.2;         // weight on seconds spent on red
    int lostTime() const { return yellow + allRed; }
};

// One traffic situation to simulate.
struct Scenario {
    std::string id, name, note;
    uint32_t seed = 42;
    int duration = 180;
    int initialQueues[4] = {0, 0, 0, 0};
    double arrivalRates[4] = {0, 0, 0, 0};  // expected cars per second
    double serviceRates[4] = {1, 1, 1, 1};  // cars per second that can leave on green
    PhaseId initialPhase = NS;
    int initialGreenRemaining = 25;
};

// Live state of one approach.
struct Approach {
    int queue = 0;
    double arrivalRate = 0;
    double serviceRate = 1;
    long long accumulatedWait = 0;  // car-seconds spent waiting
    int maxQueue = 0;
    int served = 0;
    int arrived = 0;
    int redTime = 0;    // seconds since this approach last had green
    double credit = 0;  // fractional service capacity carried between seconds
};

// The traffic light.
struct Signal {
    PhaseId phase = NS;
    Light state = Light::Green;
    int remaining = 0;     // seconds left in the current light
    int greenElapsed = 0;  // seconds the current green has lasted
    int greenPlanned = 0;  // total length planned for the current green
    bool hasNext = false;  // during yellow/all-red: which green comes next
    PhaseId nextPhase = NS;
    int nextDuration = 0;
};

// What a controller is allowed to see. Never the future arrivals.
struct ApproachView {
    int queue = 0;
    int redTime = 0;
    double arrivalRate = 0;
    double serviceRate = 1;
};
struct ControllerView {
    int time = 0;
    PhaseId phase = NS;
    Light state = Light::Green;
    int greenElapsed = 0;
    ApproachView a[4];
};

// Explanation data attached to every decision.
struct ApproachScore {
    int queue = 0;
    double predicted = 0;
    int wait = 0;
    double score = 0;
};
struct RankedPhase {
    PhaseId phase = NS;
    double score = 0;
    double priority = 0;
    bool forced = false;
    int order = 0;
    std::vector<int> overdue;
};

// A controller's answer when a green ends: which phase next and for how long.
struct Decision {
    int time = 0;
    PhaseId from = NS;
    PhaseId phase = NS;
    int duration = 0;
    std::string kind;  // "switch" or "extend"
    std::string strategy;
    std::string explanation;
    std::vector<std::string> constraints;
    bool hasScores = false;
    ApproachScore scores[4];
    std::vector<RankedPhase> ranking;
    bool hasAlloc = false;
    int alloc[2] = {0, 0};
    std::string overridden;
};
