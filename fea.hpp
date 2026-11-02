// Task C: 1-D steady thermal FEM, two-node linear elements.
#pragma once
#include <vector>
#include <stdexcept>
namespace wb {

struct RodParams {
    double L = 1.0;      // m
    double A = 1e-4;     // m^2
    double k = 10.0;     // W/(m K)
    double q = 1.0e4;    // W/m^3 (uniform volumetric generation)
    double T0 = 100.0;   // C at x = 0
    double TL = 20.0;    // C at x = L
    int    n = 10;       // number of elements
};

struct FeaResult { std::vector<double> x, T; };

// Throws std::invalid_argument with an actionable message.
void validate(const RodParams& p);
FeaResult solveRod(const RodParams& p);
// Allocation-light variant used by the batch runner (T resized to n+1).
void solveRodInto(const RodParams& p, std::vector<double>& T);

double exactT(const RodParams& p, double x);
double interpAt(const FeaResult& r, double x);       // FE (linear) interpolation
double maxNodalError(const RodParams& p, const FeaResult& r);
double maxMidpointError(const RodParams& p, const FeaResult& r);  // FE interpolation at element midpoints
}
