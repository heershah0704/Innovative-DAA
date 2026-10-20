// arrivals.hpp
// Builds the full list of arrivals before the run starts. Every strategy is
// given this same list, which is what makes the comparison fair.
#pragma once
#include <array>
#include <cstdio>
#include <string>
#include <vector>
#include "model.hpp"
#include "random.hpp"

// schedule[t][d] = number of cars arriving on approach d during second t
using Schedule = std::vector<std::array<int, 4>>;

inline Schedule generateArrivals(const Scenario& s) {
    // One independent stream per approach: changing North's rate does not
    // change what East, South or West receive.
    std::vector<Mulberry32> rng;
    for (int d = 0; d < 4; ++d) rng.emplace_back(s.seed ^ fnv1a(std::string("approach-") + dirCode(d)));

    Schedule schedule(s.duration);
    for (int t = 0; t < s.duration; ++t)
        for (int d = 0; d < 4; ++d) schedule[t][d] = poisson(rng[d], s.arrivalRates[d]);
    return schedule;
}

inline int totalArrivals(const Schedule& schedule) {
    int n = 0;
    for (const auto& row : schedule) for (int v : row) n += v;
    return n;
}

// Short ID for a schedule, shown in the UI to prove two runs had the same cars.
inline std::string fingerprint(const Schedule& schedule) {
    std::string flat;
    for (const auto& row : schedule) {
        for (int d = 0; d < 4; ++d) { flat += std::to_string(row[d]); flat += d < 3 ? ',' : ';'; }
    }
    char buf[9];
    std::snprintf(buf, sizeof buf, "%08x", fnv1a(flat));
    return buf;
}
