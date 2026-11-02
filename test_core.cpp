// Minimal dependency-free test runner. Build: see README (target wb_tests).
#include "../src/core/fea.hpp"
#include "../src/core/batch.hpp"
#include "../src/core/formats.hpp"
#include "../src/core/backprop.hpp"
#include "../src/core/devices.hpp"
#include <cmath>
#include <cstdio>
#include <functional>
using namespace wb;
static int fails = 0, total = 0;
#define CHECK(c) do { ++total; if (!(c)) { ++fails; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static bool throwsParse(const std::function<void()>& f, const char* needle) {
    try { f(); } catch (const ParseError& e) { return std::string(e.what()).find(needle) != std::string::npos; } return false;
}
static const char* kStl = "solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n  endloop\n endfacet\nendsolid t\n";

int main() {
    // ---- FEA (positive / numerical) ----
    for (int n : {10, 20, 40, 80}) {
        RodParams p; p.n = n; FeaResult r = solveRod(p);
        double h = p.L / n;
        CHECK(std::fabs(r.T.front() - 100.0) < 1e-12 && std::fabs(r.T.back() - 20.0) < 1e-12);   // BCs
        CHECK(maxNodalError(p, r) < 1e-9);                                                         // nodally exact for 1-D linear FE
        CHECK(std::fabs(maxMidpointError(p, r) - p.q / (2 * p.k) * h * h / 4.0) < 1e-9);          // analytic interpolation error 125 h^2
    }
    { RodParams a, b; a.n = 10; b.n = 20; double e1 = maxMidpointError(a, solveRod(a)), e2 = maxMidpointError(b, solveRod(b)); CHECK(std::fabs(e1 / e2 - 4.0) < 1e-6); } // O(h^2)
    { RodParams p; p.n = 1; auto r = solveRod(p); CHECK(r.T.size() == 2 && r.T[0] == 100 && r.T[1] == 20); }   // single element edge case
    { RodParams p; p.q = 0; p.n = 4; auto r = solveRod(p); CHECK(std::fabs(r.T[2] - 60.0) < 1e-12); }        // pure conduction -> linear
    // ---- FEA (negative) ----
    { RodParams p; p.k = 0;  bool t = false; try { solveRod(p); } catch (const std::invalid_argument&) { t = true; } CHECK(t); }
    { RodParams p; p.n = 0;  bool t = false; try { solveRod(p); } catch (const std::invalid_argument&) { t = true; } CHECK(t); }
    { RodParams p; p.T0 = NAN; bool t = false; try { solveRod(p); } catch (const std::invalid_argument&) { t = true; } CHECK(t); }
    // ---- Batch ----
    { auto c1 = generateCases(42, 50), c2 = generateCases(42, 50), c3 = generateCases(43, 50);
      CHECK(c1[7].k == c2[7].k && c1[7].q == c2[7].q); CHECK(c1[7].k != c3[7].k);
      for (auto& c : c1) CHECK(c.k > 0 && c.q > 0);
      RodParams base; base.n = 200; auto s = runSerial(c1, base);
      for (int w : {1, 2, 4, 8}) { auto p = runParallel(c1, base, w); CHECK(p.completed && maxAbsDiff(s, p) <= 1e-8); }
      std::atomic<bool> cancel{true}; BatchControl ctl; ctl.cancel = &cancel;
      auto x = runParallel(c1, base, 2, ctl); CHECK(!x.completed && x.T.empty());   // safe cancellation: no partial results
      bool t = false; try { runParallel(c1, base, 0); } catch (const std::invalid_argument&) { t = true; } CHECK(t); }
    // ---- STL ----
    { auto d = parseAsciiStl(kStl); CHECK(d.triangles == 1 && d.bmax[0] == 1 && d.v.size() == 9); }
    CHECK(throwsParse([] { parseAsciiStl(""); }, "empty"));
    CHECK(throwsParse([] { parseAsciiStl("solid x\nendsolid x\n"); }, "no facets"));
    CHECK(throwsParse([] { parseAsciiStl("hello"); }, "start with"));
    CHECK(throwsParse([] { parseAsciiStl(std::string("solid b\0\0", 9)); }, "binary"));
    CHECK(throwsParse([] { std::string s = kStl; s.replace(s.find("vertex 1 0 0"), 12, "vertex nan 0 0"); parseAsciiStl(s); }, "finite"));
    CHECK(throwsParse([] { std::string s = kStl; s.replace(s.find("vertex 1 0 0"), 12, "vertex 1e999 0 0"); parseAsciiStl(s); }, "finite"));
    CHECK(throwsParse([] { std::string s = kStl; s.erase(s.find("endloop"), 8); parseAsciiStl(s); }, "endloop"));
    // ---- CSV ----
    const std::string H = "node_id,x_m,y_m,z_m,temperature_C\n";
    { auto d = parseNodalCsv(H + "1,0,0,0,100\n2,1,0,0,20.5\r\n\n"); CHECK(d.id.size() == 2 && d.Tmin == 20.5 && d.Tmax == 100); }
    CHECK(throwsParse([] { parseNodalCsv(""); }, "empty"));
    CHECK(throwsParse([&] { parseNodalCsv(H); }, "no data rows"));
    CHECK(throwsParse([&] { parseNodalCsv("id,x,y,z,t\n1,0,0,0,1\n"); }, "header"));
    CHECK(throwsParse([&] { parseNodalCsv(H + "1,0,0,0\n"); }, "5 columns"));
    CHECK(throwsParse([&] { parseNodalCsv(H + "1,0,0,0,nan\n"); }, "finite"));
    CHECK(throwsParse([&] { parseNodalCsv(H + "1,0,0,0,inf\n"); }, "finite"));
    CHECK(throwsParse([&] { parseNodalCsv(H + "1,0,0,0,abc\n"); }, "finite"));
    CHECK(throwsParse([&] { parseNodalCsv(H + "1.5,0,0,0,1\n"); }, "integer"));
    CHECK(throwsParse([&] { parseNodalCsv(H + "1,0,0,0,1\n1,1,1,1,2\n"); }, "duplicate"));
    // ---- Backprop ----
    { BP p; auto a = bpGrad(p, 1, 2), n = bpNumericGrad(p, 1, 2, 1e-6);
      CHECK(std::fabs(a.dw - n.dw) <= 1e-5 && std::fabs(a.db - n.db) <= 1e-5 && std::fabs(a.dv - n.dv) <= 1e-5 && std::fabs(a.dc - n.dc) <= 1e-5);
      auto L = bpTrain(p, 1, 2, 0.1, 100); CHECK(L.size() == 101 && L.back() < L.front() * 0.5);
      auto q = bpStep(p, a, 0.1); CHECK(q.w > p.w && q.v > p.v); }   // gradients negative -> parameters rise
    // ---- Mock devices ----
    { MockBench b; CHECK(b.discover().size() == 3);
      Telemetry t; CHECK(b.telemetry("mcu0", t).code == "NOT_CONNECTED");
      CHECK(b.connect("mcu0").ok && b.telemetry("mcu0", t).ok && !t.fields.empty());
      auto r1 = b.connect("fpga0"); CHECK(!r1.ok && r1.code == "TIMEOUT");     // injected failure
      CHECK(b.connect("fpga0").ok);                                              // recovery on retry
      CHECK(b.connect("jtag0").ok && b.disconnect("jtag0").ok); CHECK(b.connect("nope").code == "UNKNOWN_DEVICE"); }
    std::printf("%d checks, %d failed\n", total, fails);
    return fails ? 1 : 0;
}
