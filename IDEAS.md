# Boot Chain Lab — future lab backlog

Defensive/analysis-only, same house style. Captured 2026-10-04. Nothing built yet.

## Lab 7 · Evidence timeline  (already the "soon" slot)
Tie Vertical + Horizontal in: take the findings from Labs 3–6 and lay them on one
time axis — persistence (L1), hooks (L2), hidden proc (L3/L6), beacon (L4), the
wire match (L5), the injected page (L6) — showing how scattered signals become one
case. The hash-chained .vbook as the evidence spine.

## NOP Sled Lab  (Michael's idea — strong candidate)
The sled has shown up twice already (keyed on in L5's wire signature, found in L6's
RAM scan). A dedicated lab unifies it, defensively:
  - What a sled is and why it exists: a landing zone so an imprecise jump still
    reaches the payload. Purely the detection side — no exploit built.
  - Classic vs evasive sleds: solid 0x90 runs → multi-byte NOP-equivalents,
    trampoline/jump sleds, register-noise sleds that defeat a naive 0x90 scan.
  - Interactive: a run-length + NOP-equivalence scanner over a byte buffer —
    find the sled by the length of its executable-but-inert run and the entropy
    "valley" it creates, not by a single byte value. Show it beating a
    string-edited variant that a literal 0x90 content-match misses.
  - Honest limit: sleds are getting shorter/rarer (precise exploits, JOP/ROP have
    no sled at all) — so this is one signal among many, same lesson as L5.
  - Cite: Fogla/Lee polymorphic blending; Akritidis et al. STRIDE sled detection;
    the generic "NOP-equivalent instruction" class.

## Other candidates
- Lab · PE/Mach-O anomaly triage — section perms, entropy per section, RWX sections,
  tiny/odd headers; the static complement to L6's runtime view.
- Lab · ROP/JOP without a sled — why gadget chains leave a different trace (stack
  pivots, misaligned returns); detection via shadow stack / CFG telemetry. Defensive.
- Lab · Persistence sweep (macOS) — LaunchAgents/Daemons, login items, TCC abuse;
  the real-world L1 on his own platform, osquery-driven.
- Lab · DNS deep-dive — expand L4's tunnel detector into DGA scoring + fast-flux.
