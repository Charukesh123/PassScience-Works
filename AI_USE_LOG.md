# AI-use log (honest record - edit/extend with your own interactions; do not fabricate)
Tool: Claude (Anthropic), chat interface with code execution; model identified by the assistant as Claude Sonnet 5.5. Single working session, 2 November 2026.
**Representative prompt (verbatim intent):** uploaded the assignment PDF with "do this full assignment for me and give me final output".
**What the AI produced:** all code in src/, tests/, tools/, CMake/VS Code configs, data generator, docs drafts, plots; it ran compile/tests/benchmarks in a Linux sandbox (1 CPU, no Windows toolchain, no browser).
**Validation evidence the AI generated:** 104 unit checks incl. analytic 125h^2 check; sanitizer builds; curl API tests; gradient check vs finite differences; benchmark agreement 0 difference.
**Incorrect/unsuitable output found during the session (real, not staged):**
1. Draft BC handling indexed f[1] and f[n-1] unconditionally -> wrong for n=1; caught on review, fixed, test added.
2. `sysinfo.cpp` crashed (`std::string::substr` out of range) on `/proc/cpuinfo` lines with empty values; found only when the benchmark CLI threw, fixed.
3. Hand-typed constants in the draft backprop write-up (1 - tanh^2 = 0.7864472 and tanh^2 = 0.2135528) were wrong in the 7th digit; caught by recomputing with Python, corrected to 0.7864477 / 0.2135523.
4. A first benchmark smoke run (200 cases, 2 reps) showed a 2x "speedup" on a 1-CPU machine; judged to be cold-start noise, not real, and documented.
**Your edits / defence notes:** [CANDIDATE: list what you changed, what you re-derived by hand, and add your own prompts]. You must be able to explain assemble/BC elimination, why midpoint error is O(h^2), the worker pool, and every backprop derivative.
**Challenge test (optional extra):** change k or q in the UI and confirm the solver still matches the analytic formula; the tests assert this only for the assignment parameters plus q=0.
