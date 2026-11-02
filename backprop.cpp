#include "backprop.hpp"
#include <cmath>
namespace wb {
BPForward bpForward(const BP& p, double x, double y) {
    BPForward f; f.z = p.w * x + p.b; f.h = std::tanh(f.z); f.yhat = p.v * f.h + p.c;
    double e = f.yhat - y; f.J = 0.5 * e * e; return f;
}
BPGrad bpGrad(const BP& p, double x, double y) {
    BPForward f = bpForward(p, x, y);
    double dyhat = f.yhat - y;            // dJ/dyhat
    double dh = dyhat * p.v;              // dJ/dh = dyhat * v
    double dz = dh * (1.0 - f.h * f.h);   // dJ/dz = dh * (1 - tanh^2 z)
    return { dz * x, dz, dyhat * f.h, dyhat };
}
BPGrad bpNumericGrad(const BP& p, double x, double y, double eps) {
    auto J = [&](BP q) { return bpForward(q, x, y).J; };
    BPGrad g;
    { BP a = p, b = p; a.w += eps; b.w -= eps; g.dw = (J(a) - J(b)) / (2 * eps); }
    { BP a = p, b = p; a.b += eps; b.b -= eps; g.db = (J(a) - J(b)) / (2 * eps); }
    { BP a = p, b = p; a.v += eps; b.v -= eps; g.dv = (J(a) - J(b)) / (2 * eps); }
    { BP a = p, b = p; a.c += eps; b.c -= eps; g.dc = (J(a) - J(b)) / (2 * eps); }
    return g;
}
BP bpStep(const BP& p, const BPGrad& g, double lr) { return { p.w - lr * g.dw, p.b - lr * g.db, p.v - lr * g.dv, p.c - lr * g.dc }; }
std::vector<double> bpTrain(BP p, double x, double y, double lr, int steps) {
    std::vector<double> L; L.push_back(bpForward(p, x, y).J);
    for (int i = 0; i < steps; ++i) { p = bpStep(p, bpGrad(p, x, y), lr); L.push_back(bpForward(p, x, y).J); }
    return L;
}
}
