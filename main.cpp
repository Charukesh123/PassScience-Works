// Presentation/transport layer: HTTP+JSON API over the C++ core, serves ui/index.html, opens the browser.
#include "http.hpp"
#include "json.hpp"
#include "../core/backprop.hpp"
#include "../core/batch.hpp"
#include "../core/devices.hpp"
#include "../core/fea.hpp"
#include "../core/formats.hpp"
#include "../core/sysinfo.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif
using namespace wb;
using Clock = std::chrono::steady_clock;
static const Clock::time_point g_start = Clock::now();
static double g_readyMs = 0;
static std::string g_ui, g_data;

// ---------- helpers ----------
static std::string exeDir() {
#ifdef _WIN32
    char b[MAX_PATH]; DWORD n = GetModuleFileNameA(nullptr, b, MAX_PATH); std::string s(b, n);
#else
    char b[4096]; ssize_t n = readlink("/proc/self/exe", b, sizeof b - 1); std::string s(b, n > 0 ? n : 0);
#endif
    size_t p = s.find_last_of("/\\"); return p == std::string::npos ? "." : s.substr(0, p);
}
static std::string slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
static double qd(const Request& r, const char* k, double def) {
    auto it = r.query.find(k); if (it == r.query.end() || it->second.empty()) return def;
    char* e = nullptr; double v = std::strtod(it->second.c_str(), &e);
    if (*e || !std::isfinite(v)) throw std::invalid_argument(std::string("Parameter '") + k + "' must be a finite number, got '" + it->second + "'.");
    return v;
}
static Response err(int code, const std::string& m) { Response r; r.status = code; r.body = "{\"error\":" + wbj::str(m) + "}"; return r; }
static Response ok(const std::string& b) { Response r; r.body = b; return r; }
static Response csv(const std::string& b) { Response r; r.type = "text/csv"; r.body = b; return r; }

// ---------- FEA ----------
static RodParams rodFrom(const Request& r) {
    RodParams p; p.L = qd(r, "L", p.L); p.A = qd(r, "A", p.A); p.k = qd(r, "k", p.k); p.q = qd(r, "q", p.q);
    p.T0 = qd(r, "T0", p.T0); p.TL = qd(r, "TL", p.TL); p.n = (int)qd(r, "n", p.n); return p;
}
static Response feaConvergence(const Request& rq) {
    RodParams base = rodFrom(rq); validate(base);
    std::string o = "{\"meshes\":[";
    bool first = true;
    for (int n : {10, 20, 40, 80}) {
        RodParams p = base; p.n = n; auto r = solveRod(p);
        if (!first) o += ',';
        first = false;
        o += "{\"n\":" + std::to_string(n) + ",\"x\":" + wbj::arr(r.x) + ",\"T\":" + wbj::arr(r.T) + ",\"err_nodes\":" + wbj::num(maxNodalError(p, r)) + ",\"err_mid\":" + wbj::num(maxMidpointError(p, r)) + "}";
    }
    std::vector<double> xs, ts; for (int i = 0; i <= 200; ++i) { double x = base.L * i / 200; xs.push_back(x); ts.push_back(exactT(base, x)); }
    return ok(o + "],\"exact\":{\"x\":" + wbj::arr(xs) + ",\"T\":" + wbj::arr(ts) + "}}");
}
static Response feaCsv(const Request& rq, bool summary) {
    RodParams base = rodFrom(rq); validate(base); std::ostringstream o; o.precision(15);
    if (!summary) o << "N_elements,x_m,T_fe_C,T_exact_C\n"; else o << "N_elements,h_m,max_abs_error_nodes_C,max_abs_error_midpoints_C\n";
    for (int n : {10, 20, 40, 80}) {
        RodParams p = base; p.n = n; auto r = solveRod(p);
        if (summary) o << n << ',' << p.L / n << ',' << maxNodalError(p, r) << ',' << maxMidpointError(p, r) << '\n';
        else for (int i = 0; i <= n; ++i) o << n << ',' << r.x[i] << ',' << r.T[i] << ',' << exactT(p, r.x[i]) << '\n';
    }
    return csv(o.str());
}

// ---------- file import ----------
static Response importStl(const Request& rq) {
    StlData d = parseAsciiStl(rq.body);
    std::string o = "{\"triangles\":" + std::to_string(d.triangles) + ",\"units\":\"m (assumed; STL carries no units)\",\"bmin\":" + wbj::arr(std::vector<double>(d.bmin, d.bmin + 3)) + ",\"bmax\":" + wbj::arr(std::vector<double>(d.bmax, d.bmax + 3)) + ",\"v\":" + wbj::arr(d.v) + "}";
    return ok(o);
}
static Response importCsv(const Request& rq) {
    NodeData d = parseNodalCsv(rq.body);
    return ok("{\"count\":" + std::to_string(d.id.size()) + ",\"Tmin\":" + wbj::num(d.Tmin) + ",\"Tmax\":" + wbj::num(d.Tmax) + ",\"units\":{\"xyz\":\"m\",\"T\":\"C\"},\"id\":" + wbj::arr(d.id) + ",\"x\":" + wbj::arr(d.x) + ",\"y\":" + wbj::arr(d.y) + ",\"z\":" + wbj::arr(d.z) + ",\"T\":" + wbj::arr(d.T) + "}");
}

// ---------- benchmark job (background thread, cancellable) ----------
struct Job {
    std::mutex m; std::atomic<bool> running{false}, cancel{false}; std::atomic<long> done{0}; long total = 0;
    std::string state = "idle", result = "null", message;
} g_job;

static void benchThread(int cases, int n, int reps, uint64_t seed, int maxW, bool oversub) {
    auto fail = [&](const std::string& s, const std::string& m) { std::lock_guard<std::mutex> l(g_job.m); g_job.state = s; g_job.message = m; g_job.running = false; };
    try {
        SysInfo si = querySysInfo(); RodParams base; base.n = n; auto cs = generateCases(seed, cases);
        std::vector<int> ws; for (int w : {1, 2, 4}) if (w <= maxW && (oversub || w <= si.logical)) ws.push_back(w);
        BatchControl ctl; ctl.cancel = &g_job.cancel; ctl.progress = &g_job.done;
        BatchResult ref; std::string rows; double T1 = 0;
        for (int w : ws) {
            auto run = [&] { return w == 1 ? runSerial(cs, base, ctl) : runParallel(cs, base, w, ctl); };
            BatchResult res = run();                                   // warm-up (excluded from timing)
            if (!res.completed) return fail("cancelled", "Cancelled safely; no partial results were kept.");
            if (w == 1) ref = std::move(res);
            std::vector<double> t; double diff = 0;
            for (int r = 0; r < reps; ++r) {
                auto a = Clock::now(); res = run(); auto b = Clock::now();
                if (!res.completed) return fail("cancelled", "Cancelled safely; no partial results were kept.");
                t.push_back(std::chrono::duration<double>(b - a).count());
            }
            diff = maxAbsDiff(ref, res);
            std::vector<double> s = t; std::sort(s.begin(), s.end()); double med = s[s.size() / 2]; if (w == 1) T1 = med;
            if (!rows.empty()) rows += ',';
            rows += "{\"workers\":" + std::to_string(w) + ",\"median_s\":" + wbj::num(med) + ",\"speedup\":" + wbj::num(T1 / med) + ",\"efficiency\":" + wbj::num(T1 / med / w) + ",\"max_abs_diff_C\":" + wbj::num(diff) + ",\"agree\":" + (diff <= 1e-8 ? "true" : "false") + ",\"oversubscribed\":" + (w > si.logical ? "true" : "false") + ",\"reps_s\":" + wbj::arr(t) + "}";
        }
        std::string r = "{\"system\":{\"cpu\":" + wbj::str(si.cpu) + ",\"physical\":" + std::to_string(si.physical) + ",\"logical\":" + std::to_string(si.logical) + ",\"ram_GiB\":" + wbj::num(si.ramGiB) + ",\"compiler\":" + wbj::str(si.compiler) + ",\"flags\":" + wbj::str(si.flags) + "},\"workload\":{\"cases\":" + std::to_string(cases) + ",\"elements\":" + std::to_string(n) + ",\"seed\":" + std::to_string(seed) + ",\"warmup\":1,\"reps\":" + std::to_string(reps) + "},\"launch_to_ready_ms\":" + wbj::num(g_readyMs) + ",\"rows\":[" + rows + "]}";
        std::lock_guard<std::mutex> l(g_job.m); g_job.result = r; g_job.state = "done"; g_job.running = false;
    } catch (const std::exception& e) { fail("error", e.what()); }
}
static Response benchStart(const Request& rq) {
    int cases = (int)qd(rq, "cases", 2000), n = (int)qd(rq, "n", 2000), reps = (int)qd(rq, "reps", 5); uint64_t seed = (uint64_t)qd(rq, "seed", 42);
    if (cases < 1 || cases > 200000) return err(400, "cases must be in [1, 200000].");
    if (n < 1 || n > 1000000) return err(400, "elements must be in [1, 1,000,000].");
    if (reps < 1 || reps > 20) return err(400, "repetitions must be in [1, 20].");
    if ((double)cases * (n + 1) * 8.0 * 2.0 > 1.5e9) return err(400, "Workload needs more than ~1.5 GB of memory (cases x elements). Reduce cases or elements.");
    bool expected = false; if (!g_job.running.compare_exchange_strong(expected, true)) return err(400, "A benchmark is already running. Cancel it or wait for completion.");
    SysInfo si = querySysInfo(); bool over = qd(rq, "oversub", 0) != 0;
    int nw = 0; for (int w : {1, 2, 4}) if (over || w <= si.logical) ++nw;
    { std::lock_guard<std::mutex> l(g_job.m); g_job.state = "running"; g_job.message.clear(); g_job.result = "null"; g_job.done = 0; g_job.cancel = false; g_job.total = (long)cases * (reps + 1) * nw; }
    std::thread(benchThread, cases, n, reps, seed, 4, over).detach();
    return ok("{\"started\":true}");
}
static Response jobStatus() {
    std::lock_guard<std::mutex> l(g_job.m);
    return ok("{\"state\":" + wbj::str(g_job.state) + ",\"done\":" + std::to_string(g_job.done.load()) + ",\"total\":" + std::to_string(g_job.total) + ",\"message\":" + wbj::str(g_job.message) + ",\"result\":" + g_job.result + "}");
}

// ---------- devices (SIMULATED) ----------
static MockBench g_bench; static std::mutex g_devMu;
static std::string devJson() {
    std::string o = "{\"simulated\":true,\"devices\":[";
    bool f = true; for (auto& d : g_bench.discover()) {
        if (!f) o += ',';
        f = false;
        o += "{\"id\":" + wbj::str(d.id) + ",\"name\":" + wbj::str(d.name) + ",\"role\":" + wbj::str(d.role) + ",\"interface\":" + wbj::str(d.interfaceDesc) + ",\"state\":" + wbj::str(d.state) + ",\"capabilities\":[";
        for (size_t i = 0; i < d.capabilities.size(); ++i) o += (i ? "," : "") + wbj::str(d.capabilities[i]);
        o += "]}";
    }
    return o + "],\"log\":" + wbj::str(g_bench.log()) + "}";
}
static Response devices(const Request& rq) {
    std::lock_guard<std::mutex> l(g_devMu); std::string id = rq.query.count("id") ? rq.query.at("id") : "";
    if (rq.path == "/api/devices") return ok(devJson());
    OpResult r; std::string tel = "null";
    if (rq.path == "/api/device/connect") r = g_bench.connect(id);
    else if (rq.path == "/api/device/disconnect") r = g_bench.disconnect(id);
    else { Telemetry t; r = g_bench.telemetry(id, t); if (r.ok) { tel = "{\"device\":" + wbj::str(t.device) + ",\"seq\":" + std::to_string(t.seq) + ",\"fields\":{"; for (size_t i = 0; i < t.fields.size(); ++i) tel += (i ? "," : "") + wbj::str(t.fields[i].first) + ":" + wbj::str(t.fields[i].second); tel += "}}"; } }
    return ok("{\"ok\":" + std::string(r.ok ? "true" : "false") + ",\"code\":" + wbj::str(r.code) + ",\"message\":" + wbj::str(r.message) + ",\"telemetry\":" + tel + ",\"state\":" + devJson() + "}");
}

// ---------- backprop ----------
static Response backprop() {
    BP p; auto f = bpForward(p, 1, 2); auto g = bpGrad(p, 1, 2); auto n = bpNumericGrad(p, 1, 2, 1e-6); auto u = bpStep(p, g, 0.1); auto L = bpTrain(p, 1, 2, 0.1, 100);
    auto G = [](BPGrad x) { return "{\"dw\":" + wbj::num(x.dw) + ",\"db\":" + wbj::num(x.db) + ",\"dv\":" + wbj::num(x.dv) + ",\"dc\":" + wbj::num(x.dc) + "}"; };
    double e = std::max({std::fabs(g.dw - n.dw), std::fabs(g.db - n.db), std::fabs(g.dv - n.dv), std::fabs(g.dc - n.dc)});
    return ok("{\"forward\":{\"z\":" + wbj::num(f.z) + ",\"h\":" + wbj::num(f.h) + ",\"yhat\":" + wbj::num(f.yhat) + ",\"J\":" + wbj::num(f.J) + "},\"analytic\":" + G(g) + ",\"numeric\":" + G(n) + ",\"max_err\":" + wbj::num(e) + ",\"updated\":{\"w\":" + wbj::num(u.w) + ",\"b\":" + wbj::num(u.b) + ",\"v\":" + wbj::num(u.v) + ",\"c\":" + wbj::num(u.c) + "},\"loss\":" + wbj::arr(L) + "}");
}

// ---------- routing ----------
static Response route(const Request& rq) {
    try {
        if (rq.path == "/" || rq.path == "/index.html") { std::string h = slurp(g_ui + "/index.html"); if (h.empty()) return err(404, "ui/index.html not found next to the executable."); Response r; r.type = "text/html"; r.body = h; return r; }
        if (rq.path == "/data/rod_cylinder.stl" || rq.path == "/data/ansys_synthetic_nodal_temperature.csv") {   // bundled samples only (no path traversal)
            std::string f = slurp(g_data + rq.path.substr(5)); if (f.empty()) return err(404, "Sample file missing from data/ folder."); Response r; r.type = "text/plain"; r.body = f; return r; }
        if (rq.path == "/api/status") { SysInfo s = querySysInfo(); return ok("{\"ready_ms\":" + wbj::num(g_readyMs) + ",\"logical\":" + std::to_string(s.logical) + ",\"version\":\"1.0\"}"); }
        if (rq.path == "/api/fea/convergence") return feaConvergence(rq);
        if (rq.path == "/api/fea/csv") return feaCsv(rq, false);
        if (rq.path == "/api/fea/summary.csv") return feaCsv(rq, true);
        if (rq.path == "/api/stl" && rq.method == "POST") return importStl(rq);
        if (rq.path == "/api/csv" && rq.method == "POST") return importCsv(rq);
        if (rq.path == "/api/bench/start" && rq.method == "POST") return benchStart(rq);
        if (rq.path == "/api/job") return jobStatus();
        if (rq.path == "/api/job/cancel" && rq.method == "POST") { g_job.cancel = true; return ok("{\"cancelling\":true}"); }
        if (rq.path.rfind("/api/device", 0) == 0) return devices(rq);
        if (rq.path == "/api/backprop") return backprop();
        return err(404, "Unknown endpoint " + rq.path);
    } catch (const ParseError& e) { return err(400, e.what()); }
    catch (const std::invalid_argument& e) { return err(400, e.what()); }
    catch (const std::exception& e) { return err(500, std::string("Internal error: ") + e.what()); }
}

static int realMain(int argc, char** argv) {
    bool noBrowser = false; for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--no-browser") noBrowser = true;
    std::string dir = exeDir(); g_ui = dir + "/ui";
    if (slurp(g_ui + "/index.html").empty()) g_ui = dir + "/../src/frontend";     // dev tree fallback
    g_data = dir + "/data"; if (slurp(g_data + "/rod_cylinder.stl").empty()) g_data = dir + "/../data";
    int port = httpListen(8765);
    if (!port) {
#ifdef _WIN32
        MessageBoxA(nullptr, "Could not open a local port (8765-8785). Close other copies of the Workbench and retry.", "Unified Engineering Workbench", MB_ICONERROR);
#else
        std::fprintf(stderr, "no free port\n");
#endif
        return 1;
    }
    g_readyMs = std::chrono::duration<double, std::milli>(Clock::now() - g_start).count();
    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
    if (!noBrowser) {
#ifdef _WIN32
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
        std::string c = "xdg-open " + url + " >/dev/null 2>&1 &"; if (std::system(c.c_str())) {}
#endif
    }
    std::printf("Workbench listening on %s (ready in %.1f ms)\n", url.c_str(), g_readyMs); std::fflush(stdout);
    httpServe(route);
    return 0;
}
#if defined(_WIN32) && defined(WB_GUI)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return realMain(__argc, __argv); }
#else
int main(int argc, char** argv) { return realMain(argc, argv); }
#endif
