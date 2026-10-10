---
name: reference-substance-designer-engine-rules
description: "Measured Substance Designer 16 / sbsrender rules — gray source on colour Blend is a no-op, White Noise is resolution-dependent, blur units, Histogram Scan semantics"
metadata:
  node_type: memory
  type: reference
  originSessionId: b0898a20-3540-4116-9321-4c45e9195191
  modified: 2026-10-09T22:48:28.619Z
---

Measured 2026-10-09 with sbsrender (Designer 16.0.4) while building [[project-peeling-plaster-designer-v08]]:
- Blend with a GRAYSCALE source and COLOUR destination has no effect in any mode (gray→RGBA alpha 0; `alphablending` flag doesn't help). Modulate colour via the Blend OPACITY input instead (multiply by g = Copy-blend black with opacity 1-g). Gray-on-gray blends work.
- White Noise (white_noise_v2) is per-pixel: correlation 1K↔2K = 0 → different drawing per output size. Use fine Cells 4 random. Clouds/Perlin/Cells1/Cells4/Fractal Sum/Moisture are consistent (corr ≥ 0.96).
- Atomic Blur intensity 1 ≈ Gaussian σ 2.45/1000 of tile at every resolution; Warp is resolution-independent; Edge Detect width is in pixels.
- Histogram Scan, Contrast 1 = hard threshold at value (1 − Position); Contrast c gives a ramp of width ~(1−c). Position is not an area %.
- Graph baseParameters must NOT set absolute outputsize, or `$outputsize` in sbsrender is ignored.
- noise_cells_4_v2 color_source=2 (Image input) samples the input once per cell → per-cell quantisation of any field.
