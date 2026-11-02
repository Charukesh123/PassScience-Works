#include "fea.hpp"
#include <cmath>
#include <algorithm>
#include <string>
namespace wb {

void validate(const RodParams& p) {
    auto fin = [](double v) { return std::isfinite(v); };
    if (!fin(p.L) || p.L <= 0) throw std::invalid_argument("Length L must be a finite number > 0 (m).");
    if (!fin(p.A) || p.A <= 0) throw std::invalid_argument("Area A must be a finite number > 0 (m^2).");
    if (!fin(p.k) || p.k <= 0) throw std::invalid_argument("Conductivity k must be a finite number > 0 (W/(m K)).");
    if (!fin(p.q)) throw std::invalid_argument("Heat generation q must be finite (W/m^3).");
    if (!fin(p.T0) || !fin(p.TL)) throw std::invalid_argument("Boundary temperatures must be finite (C).");
    if (p.n < 1 || p.n > 5000000) throw std::invalid_argument("Element count n must be in [1, 5,000,000].");
}

// Assembly: element stiffness Ke = (kA/h)[[1,-1],[-1,1]], element load fe = (qAh/2)[1,1].
// The global matrix is symmetric tridiagonal (diagonal d, off-diagonal e between i and i+1).
// Dirichlet conditions are imposed by elimination: the known column is moved to the RHS and the
// row is replaced by T_i = T_known. This keeps the matrix symmetric.
void solveRodInto(const RodParams& p, std::vector<double>& T) {
    const int n = p.n, N = n + 1;
    const double h = p.L / n, kah = p.k * p.A / h, fe = p.q * p.A * h / 2.0;
    std::vector<double> d(N, 0.0), e(n, 0.0), f(N, 0.0);
    for (int el = 0; el < n; ++el) {
        d[el] += kah; d[el + 1] += kah; e[el] -= kah;
        f[el] += fe;  f[el + 1] += fe;
    }
    // Move known T0 / TL to the RHS of the neighbouring rows, then overwrite boundary rows.
    if (n >= 2) { f[1] -= e[0] * p.T0; f[n - 1] -= e[n - 1] * p.TL; }
    d[0] = 1.0; e[0] = 0.0; f[0] = p.T0;
    d[n] = 1.0; e[n - 1] = 0.0; f[n] = p.TL;
    // Thomas algorithm (sub-diagonal == super-diagonal == e)
    for (int i = 1; i < N; ++i) {
        double m = e[i - 1] / d[i - 1];
        d[i] -= m * e[i - 1];
        f[i] -= m * f[i - 1];
    }
    T.assign(N, 0.0);
    T[N - 1] = f[N - 1] / d[N - 1];
    for (int i = N - 2; i >= 0; --i) T[i] = (f[i] - e[i] * T[i + 1]) / d[i];
}

FeaResult solveRod(const RodParams& p) {
    validate(p);
    FeaResult r;
    solveRodInto(p, r.T);
    r.x.resize(p.n + 1);
    for (int i = 0; i <= p.n; ++i) r.x[i] = p.L * i / p.n;
    return r;
}

double exactT(const RodParams& p, double x) {
    return p.T0 + (p.TL - p.T0) * x / p.L + p.q / (2.0 * p.k) * x * (p.L - x);
}

double interpAt(const FeaResult& r, double x) {
    const int n = (int)r.x.size() - 1;
    double h = r.x.back() / n;
    int el = std::min(n - 1, std::max(0, (int)std::floor(x / h)));
    double xi = (x - r.x[el]) / h;
    return r.T[el] * (1 - xi) + r.T[el + 1] * xi;
}

double maxNodalError(const RodParams& p, const FeaResult& r) {
    double m = 0;
    for (size_t i = 0; i < r.x.size(); ++i) m = std::max(m, std::fabs(r.T[i] - exactT(p, r.x[i])));
    return m;
}

double maxMidpointError(const RodParams& p, const FeaResult& r) {
    double m = 0;
    for (size_t i = 0; i + 1 < r.x.size(); ++i) {
        double xm = 0.5 * (r.x[i] + r.x[i + 1]);
        m = std::max(m, std::fabs(interpAt(r, xm) - exactT(p, xm)));
    }
    return m;
}
}
