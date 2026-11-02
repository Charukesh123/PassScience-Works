// Task F: h = tanh(w x + b), yhat = v h + c, J = 0.5 (yhat - y)^2. Hand-derived gradients, no autodiff.
#pragma once
#include <vector>
namespace wb {
struct BP { double w = 0.5, b = 0.0, v = 1.0, c = 0.0; };
struct BPForward { double z, h, yhat, J; };
struct BPGrad { double dw, db, dv, dc; };
BPForward bpForward(const BP& p, double x, double y);
BPGrad bpGrad(const BP& p, double x, double y);
BPGrad bpNumericGrad(const BP& p, double x, double y, double eps = 1e-6);   // central differences
BP bpStep(const BP& p, const BPGrad& g, double lr);                         // simultaneous update
std::vector<double> bpTrain(BP p, double x, double y, double lr, int steps); // loss before step 0 .. after last step
}
