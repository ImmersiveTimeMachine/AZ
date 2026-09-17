// Custom material expression. XY ribbon: U = distance fraction; V = cross section.
// CPD0 = segment start cm, CPD1 = segment chord length cm. No per-segment UV reset.
float distanceCm = StartDistance + UV.x * max(SegmentLength, 0.0);
float spacing = max(PulseSpacing, 0.1);
float duty = clamp(PulseDuty, 0.01, 0.99);
float phase = (distanceCm + PulsePhase) / spacing;
float aaPhase = max(fwidth(phase), 0.0001);
float periodicDistance = abs(frac(phase - duty * 0.5 + 0.5) - 0.5) - duty * 0.5;
float pulse = 1.0 - smoothstep(-aaPhase, aaPhase, periodicDistance);
// Minified patterns become their average coverage rather than crawling/aliasing.
pulse = lerp(pulse, duty, saturate((aaPhase - 0.15) / 0.35));
float across = abs(UV.y - 0.5) * 2.0;
float aaWidth = max(fwidth(across), 0.001);
float strip = 1.0 - smoothstep(1.0 - aaWidth, 1.0 + aaWidth, across);
float filament = 1.0 - smoothstep(FilamentWidth - aaWidth, FilamentWidth + aaWidth, across);
float coverage = max(saturate(FilamentAlpha) * filament, saturate(PulseAlpha) * pulse * strip);
return float4(Color.rgb * Brightness, saturate(coverage * Opacity));
