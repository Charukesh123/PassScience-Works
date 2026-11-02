# Task F - worked solution to copy BY HAND onto <= 2 pages (submission requires your own handwriting; numbers are from the C++ run, results/backprop_check.txt)
Given x=1, y=2, w=0.5, b=0, v=1, c=0, lr=0.1. Model: z = wx+b, h = tanh z, y^ = v h + c, J = 1/2 (y^ - y)^2.
**Forward:** z = 0.5; h = tanh(0.5) = 0.4621172; y^ = 1*0.4621172 + 0 = 0.4621172; e = y^ - y = -1.5378828; J = 0.5 e^2 = 1.1825418.
**Backward (chain rule):** dJ/dy^ = e = -1.5378828.
dJ/dc = e * dy^/dc = e*1 = -1.5378828.   dJ/dv = e*h = -1.5378828*0.4621172 = -0.7106820.
dJ/dh = e*v = -1.5378828.   dh/dz = 1 - tanh^2 z = 1 - 0.2135523 = 0.7864477.   dJ/dz = dJ/dh * dh/dz = -1.2094645.
dJ/dw = dJ/dz * x = -1.2094645.   dJ/db = dJ/dz * 1 = -1.2094645.
**Update (all four from the ORIGINAL parameters, simultaneously):** w' = 0.5 - 0.1(-1.2094645) = 0.6209464; b' = 0 - 0.1(-1.2094645) = 0.1209464; v' = 1 - 0.1(-0.7106820) = 1.0710682; c' = 0 - 0.1(-1.5378828) = 0.1537883.
**Direction:** every gradient is negative (the prediction is too low), so subtracting lr*gradient *raises* each parameter, which raises y^ toward 2. Gradient points uphill in J; we step opposite.
**Numerical check (central difference, eps=1e-6):** all four agree with the analytic values to <= 1.2e-10 (tolerance 1e-5).
**Loss curve (100 steps, same lr):** J falls monotonically from 1.1825 (step 0) to 0.6858 (step 1) and then to ~1.7e-20 at step 100 (results/backprop_loss.png, log scale). One sample and four free parameters means the network can fit the target exactly; the descent is linear (geometric) because the gradient shrinks with the error, and the floor is floating-point round-off, not a model limit.
