// Headless evidence generator: wb_cli fea|bench|backprop <outdir>   (all numerics from the C++ core)
#include "../src/core/fea.hpp"
#include "../src/core/batch.hpp"
#include "../src/core/backprop.hpp"
#include "../src/core/sysinfo.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
using namespace wb;
using Clock = std::chrono::steady_clock;

static int cmdFea(const std::string& out) {
    std::ofstream c(out + "/fea_solution.csv"), s(out + "/fea_convergence.csv");
    c.precision(15); s.precision(15);
    c << "N_elements,x_m,T_fe_C,T_exact_C\n"; s << "N_elements,h_m,max_abs_error_nodes_C,max_abs_error_midpoints_C,observed_order\n";
    double prev = 0, prevh = 0;
    for (int n : {10, 20, 40, 80}) {
        RodParams p; p.n = n; auto r = solveRod(p);
        for (int i = 0; i <= n; ++i) c << n << ',' << r.x[i] << ',' << r.T[i] << ',' << exactT(p, r.x[i]) << '\n';
        double h = p.L / n, em = maxMidpointError(p, r);
        s << n << ',' << h << ',' << maxNodalError(p, r) << ',' << em << ',';
        if (prev > 0) s << std::log(prev / em) / std::log(prevh / h); s << '\n';
        prev = em; prevh = h;
    }
    return 0;
}
static int cmdBackprop(const std::string& out) {
    BP p; double x = 1, y = 2, lr = 0.1;
    auto f = bpForward(p, x, y); auto g = bpGrad(p, x, y); auto n = bpNumericGrad(p, x, y, 1e-6); auto q = bpStep(p, g, lr);
    std::ofstream o(out + "/backprop_check.txt"); o.precision(10);
    o << "forward: z=" << f.z << " h=" << f.h << " yhat=" << f.yhat << " J=" << f.J << "\n";
    o << "analytic : dw=" << g.dw << " db=" << g.db << " dv=" << g.dv << " dc=" << g.dc << "\n";
    o << "numeric  : dw=" << n.dw << " db=" << n.db << " dv=" << n.dv << " dc=" << n.dc << "\n";
    double e[4] = {std::fabs(g.dw - n.dw), std::fabs(g.db - n.db), std::fabs(g.dv - n.dv), std::fabs(g.dc - n.dc)};
    o << "abs err  : " << e[0] << " " << e[1] << " " << e[2] << " " << e[3] << "  (tolerance 1e-5)\n";
    o << "update(lr=0.1): w=" << q.w << " b=" << q.b << " v=" << q.v << " c=" << q.c << "\n";
    auto L = bpTrain(p, x, y, lr, 100);
    std::ofstream l(out + "/backprop_loss.csv"); l.precision(15); l << "step,loss\n";
    for (size_t i = 0; i < L.size(); ++i) l << i << ',' << L[i] << '\n';   // step 0 = initial parameters
    return *std::max_element(e, e + 4) <= 1e-5 ? 0 : 1;
}
static double median(std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; }
static int cmdBench(const std::string& out, int cases, int n, int reps, bool force) {
    SysInfo si = querySysInfo();
    RodParams base; base.n = n; auto cs = generateCases(42, cases);
    std::vector<int> ws = {1, 2, 4}; if (!force) ws.erase(std::remove_if(ws.begin(), ws.end(), [&](int w) { return w > si.logical; }), ws.end());
    std::ofstream o(out + "/benchmark.csv"); o.precision(9);
    o << "workers,median_s,speedup_T1_over_Tp,efficiency,max_abs_diff_vs_serial_C,reps_s,note\n";
    // warm-up (serial + parallel), excluded
    runSerial(cs, base); runParallel(cs, base, 2);
    auto ref = runSerial(cs, base);
    double T1 = 0;
    for (int w : ws) {
        std::vector<double> t; double diff = 0;
        for (int r = 0; r < reps; ++r) {
            auto a = Clock::now(); BatchResult res = (w == 1) ? runSerial(cs, base) : runParallel(cs, base, w); auto b = Clock::now();
            t.push_back(std::chrono::duration<double>(b - a).count());
            if (r == reps - 1) diff = maxAbsDiff(ref, res);
        }
        double m = median(t); if (w == 1) T1 = m;
        std::string note = (w > si.logical) ? "OVERSUBSCRIBED: workers > logical CPUs; correctness demo only, speedup not assessable" : "";
        o << w << ',' << m << ',' << T1 / m << ',' << (T1 / m) / w << ',' << diff << ',';
        for (size_t i = 0; i < t.size(); ++i) o << (i ? ";" : "") << t[i]; o << ',' << note << '\n';
    }
    std::ofstream m(out + "/benchmark_system.txt");
    m << "cpu: " << si.cpu << "\nphysical_cores: " << si.physical << "\nlogical_cpus: " << si.logical << "\nram_GiB: " << si.ramGiB
      << "\ncompiler: " << si.compiler << "\nflags: " << si.flags << "\nworkload: " << cases << " cases x " << n << " elements, seed 42, k in [1,100], q in [1e3,1e5]"
      << "\nprotocol: 1 warm-up (excluded), " << reps << " measured repetitions, median reported; compute only (no I/O/rendering)\n";
    return 0;
}
int main(int argc, char** argv) {
    if (argc < 3) { std::puts("usage: wb_cli fea|bench|backprop <outdir> [cases n reps] [--force-workers]"); return 2; }
    std::string c = argv[1], out = argv[2];
    try {
        if (c == "fea") return cmdFea(out);
        if (c == "backprop") return cmdBackprop(out);
        if (c == "bench") { bool f = false; for (int i = 3; i < argc; ++i) if (!std::strcmp(argv[i], "--force-workers")) f = true;
            return cmdBench(out, argc > 3 && argv[3][0] != '-' ? std::atoi(argv[3]) : 3000, argc > 4 && argv[4][0] != '-' ? std::atoi(argv[4]) : 4000, argc > 5 && argv[5][0] != '-' ? std::atoi(argv[5]) : 5, f); }
    } catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); return 1; }
    return 2;
}
