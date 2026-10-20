// random.hpp
// Deterministic random numbers. The same seed always gives the same sequence,
// so the baseline and the optimized run can see exactly the same cars.
#pragma once
#include <cmath>
#include <cstdint>
#include <string>

// mulberry32: a tiny, fast 32-bit generator. Returns numbers in [0, 1).
class Mulberry32 {
public:
    explicit Mulberry32(uint32_t seed) : state_(seed) {}

    double next() {
        state_ += 0x6D2B79F5u;
        uint32_t t = state_;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
    }

private:
    uint32_t state_;
};

// FNV-1a hash of a string. Used to give each approach its own random stream
// and to fingerprint an arrival schedule.
inline uint32_t fnv1a(const std::string& text) {
    uint32_t h = 2166136261u;
    for (unsigned char c : text) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

// Knuth's method: number of cars arriving in one second when the average is lambda.
inline int poisson(Mulberry32& rng, double lambda) {
    if (!(lambda > 0)) return 0;
    const double limit = std::exp(-lambda);
    int k = 0;
    double p = 1.0;
    do {
        ++k;
        p *= rng.next();
    } while (p > limit);
    return k - 1;
}
