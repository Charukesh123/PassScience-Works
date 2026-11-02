// Task D: independent thermal solves, serial vs bounded worker pool.
#pragma once
#include "fea.hpp"
#include <atomic>
#include <cstdint>
#include <vector>
namespace wb {
struct Case { double k, q; };

// Reproducible: own mapping of mt19937_64 output (std distributions are not portable across stdlibs).
// k in [1,100] W/(m K), q in [1e3,1e5] W/m^3.
std::vector<Case> generateCases(uint64_t seed, int count);

struct BatchControl {
    std::atomic<bool>* cancel = nullptr;     // optional
    std::atomic<long>* progress = nullptr;   // optional, incremented per finished solve
};
struct BatchResult { std::vector<std::vector<double>> T; bool completed = false; };

BatchResult runSerial(const std::vector<Case>& cases, const RodParams& base, BatchControl ctl = {});
// workers >= 1; the pool is bounded to `workers` std::threads. Results are written by input index.
BatchResult runParallel(const std::vector<Case>& cases, const RodParams& base, int workers, BatchControl ctl = {});
double maxAbsDiff(const BatchResult& a, const BatchResult& b);
}
