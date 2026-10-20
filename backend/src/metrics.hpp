// metrics.hpp
// Results of a run, all computed from what actually happened in the simulation.
//   average wait = total car-seconds of waiting / cars served   (none served -> undefined)
#pragma once
#include "engine.hpp"

struct Summary {
    int served = 0;
    int arrived = 0;
    long long totalWait = 0;
    bool hasAvgWait = false;
    double avgWait = 0;
    int residual = 0;       // cars still waiting at the end
    int maxQueue = 0;       // longest single-approach queue
    int maxQueueBy[4] = {0, 0, 0, 0};
    int maxTotalQueue = 0;  // largest total of all four queues
    int switches = 0;
};

inline Summary summarize(const Simulation& sim) {
    Summary s;
    s.served = sim.totalServed();
    s.arrived = sim.totalArrived();
    s.totalWait = sim.totalWait();
    s.hasAvgWait = s.served > 0;
    s.avgWait = s.hasAvgWait ? static_cast<double>(s.totalWait) / s.served : 0;
    s.residual = sim.totalQueue();
    for (int d = 0; d < 4; ++d) {
        s.maxQueueBy[d] = sim.approach(d).maxQueue;
        if (s.maxQueueBy[d] > s.maxQueue) s.maxQueue = s.maxQueueBy[d];
    }
    s.maxTotalQueue = sim.maxTotalQueue();
    s.switches = sim.switches();
    return s;
}
