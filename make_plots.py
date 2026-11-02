"""Plots the raw CSVs produced by wb_cli into results/*.png (analysis helper; all numbers come from the C++ core)."""
import csv, collections, matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
R = "results/"
rows = list(csv.DictReader(open(R + "fea_solution.csv"))); by = collections.defaultdict(lambda: ([], [], []))
for r in rows:
    b = by[int(r["N_elements"])]; b[0].append(float(r["x_m"])); b[1].append(float(r["T_fe_C"])); b[2].append(float(r["T_exact_C"]))
plt.figure(figsize=(8, 4.5)); ex = by[80]; plt.plot(ex[0], ex[2], "k--", lw=2, label="exact")
for n, (x, t, _) in by.items(): plt.plot(x, t, marker="o" if n <= 20 else None, ms=4, label=f"FE N={n}")
plt.xlabel("x (m)"); plt.ylabel("Temperature T (°C)"); plt.title("1-D rod: FE vs exact (L=1 m, k=10 W/m·K, q=1e4 W/m³)"); plt.legend(); plt.grid(alpha=.3); plt.tight_layout(); plt.savefig(R + "fea_temperature_vs_x.png", dpi=150)
s = list(csv.DictReader(open(R + "fea_convergence.csv"))); h = [float(r["h_m"]) for r in s]; e = [float(r["max_abs_error_midpoints_C"]) for r in s]
plt.figure(figsize=(5.5, 4.5)); plt.loglog(h, e, "o-", label="max |error| at midpoints (FE interpolation)"); plt.loglog(h, [125 * x * x for x in h], "k:", label="125 h² (analytic)")
plt.xlabel("element size h (m)"); plt.ylabel("max abs error (°C)"); plt.title("Mesh convergence (order 2)"); plt.legend(fontsize=8); plt.grid(alpha=.3, which="both"); plt.tight_layout(); plt.savefig(R + "fea_convergence.png", dpi=150)
l = [float(r["loss"]) for r in csv.DictReader(open(R + "backprop_loss.csv"))]
plt.figure(figsize=(7, 4)); plt.semilogy(range(len(l)), [max(v, 1e-20) for v in l]); plt.xlabel("training step"); plt.ylabel("loss J (log scale)"); plt.title("Backprop: x=1, y=2, lr=0.1 (step 0 = initial)"); plt.grid(alpha=.3, which="both"); plt.tight_layout(); plt.savefig(R + "backprop_loss.png", dpi=150)
b = list(csv.DictReader(open(R + "benchmark.csv"))); w = [r["workers"] for r in b]; t = [float(r["median_s"]) for r in b]
plt.figure(figsize=(5.5, 4)); plt.bar(w, t); plt.xlabel("workers (all on a 1-logical-CPU VM: speedup NOT assessable)"); plt.ylabel("median compute time (s)"); plt.title("Batch solve timing (5 reps, median)"); plt.tight_layout(); plt.savefig(R + "benchmark_times.png", dpi=150)
