#include "batch.hpp"
#include <random>
#include <thread>
#include <exception>
#include <mutex>
#include <cmath>
#include <algorithm>
namespace wb {

std::vector<Case> generateCases(uint64_t seed, int count) {
    if (count < 0) throw std::invalid_argument("Case count must be >= 0.");
    std::mt19937_64 g(seed);
    auto u01 = [&] { return (double)(g() >> 11) * (1.0 / 9007199254740992.0); };  // [0,1) from 53 bits
    std::vector<Case> c(count);
    for (auto& x : c) { x.k = 1.0 + 99.0 * u01(); x.q = 1e3 + (1e5 - 1e3) * u01(); }
    return c;
}

static void solveOne(const Case& c, const RodParams& base, std::vector<double>& out) {
    RodParams p = base; p.k = c.k; p.q = c.q;
    solveRodInto(p, out);
}

BatchResult runSerial(const std::vector<Case>& cases, const RodParams& base, BatchControl ctl) {
    validate(base);
    BatchResult r; r.T.resize(cases.size());
    for (size_t i = 0; i < cases.size(); ++i) {
        if (ctl.cancel && ctl.cancel->load()) { r.T.clear(); return r; }
        solveOne(cases[i], base, r.T[i]);
        if (ctl.progress) ctl.progress->fetch_add(1);
    }
    r.completed = true; return r;
}

BatchResult runParallel(const std::vector<Case>& cases, const RodParams& base, int workers, BatchControl ctl) {
    validate(base);
    if (workers < 1) throw std::invalid_argument("workers must be >= 1.");
    BatchResult r; r.T.resize(cases.size());
    std::atomic<size_t> next{0};
    std::atomic<bool> stop{false};
    std::exception_ptr err; std::mutex em;
    auto work = [&] {
        try {
            for (;;) {
                if (stop.load() || (ctl.cancel && ctl.cancel->load())) { stop = true; return; }
                size_t i = next.fetch_add(1);
                if (i >= cases.size()) return;
                solveOne(cases[i], base, r.T[i]);      // each task writes only its own slot -> input order
                if (ctl.progress) ctl.progress->fetch_add(1);
            }
        } catch (...) { std::lock_guard<std::mutex> l(em); if (!err) err = std::current_exception(); stop = true; }
    };
    std::vector<std::thread> pool;
    for (int i = 0; i < workers; ++i) pool.emplace_back(work);
    for (auto& t : pool) t.join();
    if (err) std::rethrow_exception(err);
    if (stop.load()) { r.T.clear(); return r; }       // cancelled: discard partial data, never return half results
    r.completed = true; return r;
}

double maxAbsDiff(const BatchResult& a, const BatchResult& b) {
    if (a.T.size() != b.T.size()) return INFINITY;
    double m = 0;
    for (size_t i = 0; i < a.T.size(); ++i) {
        if (a.T[i].size() != b.T[i].size()) return INFINITY;
        for (size_t j = 0; j < a.T[i].size(); ++j) m = std::max(m, std::fabs(a.T[i][j] - b.T[i][j]));
    }
    return m;
}
}
