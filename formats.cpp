#include "formats.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_set>
#include <algorithm>
namespace wb {
namespace {
size_t lineOf(const std::string& s, size_t pos) { return 1 + std::count(s.begin(), s.begin() + std::min(pos, s.size()), '\n'); }

struct Tok {
    const std::string& s; size_t pos = 0;
    explicit Tok(const std::string& t) : s(t) {}
    bool next(std::string& out) {
        while (pos < s.size() && std::isspace((unsigned char)s[pos])) ++pos;
        if (pos >= s.size()) return false;
        size_t b = pos; while (pos < s.size() && !std::isspace((unsigned char)s[pos])) ++pos;
        out.assign(s, b, pos - b); return true;
    }
    void skipLine() { while (pos < s.size() && s[pos] != '\n') ++pos; }
};
bool toDouble(const std::string& t, double& v) {
    if (t.empty()) return false;
    char* end = nullptr; v = std::strtod(t.c_str(), &end);
    return end == t.c_str() + t.size() && std::isfinite(v);
}
}

StlData parseAsciiStl(const std::string& text) {
    if (text.empty()) throw ParseError("STL file is empty (0 bytes). Select a file that contains an ASCII STL solid.");
    if (text.size() > kMaxInputBytes) throw ParseError("STL file exceeds the 100 MB limit of this prototype.");
    if (text.find('\0') != std::string::npos)
        throw ParseError("This looks like a binary STL (contains NUL bytes). Only ASCII STL is supported; re-export as ASCII.");
    Tok t(text); std::string w;
    if (!t.next(w) || w != "solid") throw ParseError("Not an ASCII STL: the file must start with the keyword 'solid'.");
    t.skipLine();
    StlData d; bool first = true;
    auto fail = [&](const std::string& m) { throw ParseError("STL line " + std::to_string(lineOf(text, t.pos)) + ": " + m); };
    auto expect = [&](const char* kw) { if (!t.next(w) || w != kw) fail(std::string("expected '") + kw + "'"); };
    auto num = [&]() { double v; if (!t.next(w) || !toDouble(w, v)) fail("expected a finite number, found '" + w + "'"); return v; };
    while (t.next(w)) {
        if (w == "endsolid") break;
        if (w != "facet") fail("expected 'facet' or 'endsolid', found '" + w + "'");
        expect("normal"); for (int i = 0; i < 3; ++i) num();
        expect("outer"); expect("loop");
        for (int vi = 0; vi < 3; ++vi) {
            expect("vertex");
            for (int a = 0; a < 3; ++a) {
                double c = num(); d.v.push_back((float)c);
                if (first) { d.bmin[a] = d.bmax[a] = c; } else { d.bmin[a] = std::min(d.bmin[a], c); d.bmax[a] = std::max(d.bmax[a], c); }
            }
            first = false;
        }
        expect("endloop"); expect("endfacet");
        ++d.triangles;
    }
    if (d.triangles == 0) throw ParseError("STL contains a 'solid' header but no facets, so there is nothing to display.");
    return d;
}

NodeData parseNodalCsv(const std::string& text) {
    if (text.empty()) throw ParseError("CSV file is empty (0 bytes). Expected header node_id,x_m,y_m,z_m,temperature_C.");
    if (text.size() > kMaxInputBytes) throw ParseError("CSV file exceeds the 100 MB limit of this prototype.");
    size_t pos = 0, line = 0; NodeData d; std::unordered_set<long long> seen; bool header = false;
    while (pos < text.size()) {
        size_t e = text.find('\n', pos); if (e == std::string::npos) e = text.size();
        std::string ln = text.substr(pos, e - pos); pos = e + 1; ++line;
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        if (line == 1 && ln.size() >= 3 && (unsigned char)ln[0] == 0xEF) ln.erase(0, 3);   // UTF-8 BOM
        if (ln.find_first_not_of(" \t") == std::string::npos) continue;                    // blank line
        std::vector<std::string> f; size_t b = 0;
        for (;;) { size_t c = ln.find(',', b); f.push_back(ln.substr(b, c == std::string::npos ? c : c - b)); if (c == std::string::npos) break; b = c + 1; }
        for (auto& s : f) { size_t a = s.find_first_not_of(" \t"), z = s.find_last_not_of(" \t"); s = a == std::string::npos ? "" : s.substr(a, z - a + 1); }
        auto fail = [&](const std::string& m) { throw ParseError("CSV line " + std::to_string(line) + ": " + m); };
        if (!header) {
            if (f.size() != 5 || f[0] != "node_id" || f[1] != "x_m" || f[2] != "y_m" || f[3] != "z_m" || f[4] != "temperature_C")
                fail("header must be exactly node_id,x_m,y_m,z_m,temperature_C");
            header = true; continue;
        }
        if (f.size() != 5) fail("expected 5 columns, found " + std::to_string(f.size()));
        char* end = nullptr; long long id = std::strtoll(f[0].c_str(), &end, 10);
        if (f[0].empty() || *end) fail("node_id '" + f[0] + "' is not an integer");
        if (!seen.insert(id).second) fail("duplicate node_id " + f[0]);
        double v[4];
        for (int i = 0; i < 4; ++i) if (!toDouble(f[i + 1], v[i])) fail("column " + std::to_string(i + 2) + " value '" + f[i + 1] + "' is not a finite number (NaN/Inf rejected)");
        d.id.push_back(id); d.x.push_back(v[0]); d.y.push_back(v[1]); d.z.push_back(v[2]); d.T.push_back(v[3]);
    }
    if (!header) throw ParseError("CSV contains no header row.");
    if (d.id.empty()) throw ParseError("CSV has a valid header but no data rows, so there are no nodes to display.");
    d.Tmin = *std::min_element(d.T.begin(), d.T.end()); d.Tmax = *std::max_element(d.T.begin(), d.T.end());
    return d;
}
}
