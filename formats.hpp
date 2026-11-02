// Task B: ASCII STL and ANSYS-style nodal-temperature CSV parsing + validation.
#pragma once
#include <string>
#include <vector>
#include <stdexcept>
namespace wb {
struct ParseError : std::runtime_error { using std::runtime_error::runtime_error; };

constexpr size_t kMaxInputBytes = 100u * 1024u * 1024u;   // hard cap (tested limits in docs/)

struct StlData {
    std::vector<float> v;          // 9 floats per triangle (3 vertices xyz)
    size_t triangles = 0;
    double bmin[3] = {0,0,0}, bmax[3] = {0,0,0};
};
struct NodeData {
    std::vector<long long> id;
    std::vector<double> x, y, z, T;
    double Tmin = 0, Tmax = 0;
};
StlData parseAsciiStl(const std::string& text);     // throws ParseError
NodeData parseNodalCsv(const std::string& text);    // throws ParseError
}
