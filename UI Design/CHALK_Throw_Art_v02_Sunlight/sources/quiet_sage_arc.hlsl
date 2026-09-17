// Quiet Sage daylight revision. The 5.5px ribbon contains a 3.5px colored stroke
// plus a charcoal keyline. Existing CPD0/1 distance and pulse semantics are kept.
float distanceCm = StartDistance + UV.x * max(SegmentLength, 0.0);
float spacing = max(PulseSpacing, 0.1);
float duty = clamp(PulseDuty, 0.01, 0.99);
float phase = (distanceCm + PulsePhase) / spacing;
float aaPhase = max(fwidth(phase), 0.0001);
float pd = abs(frac(phase - duty * 0.5 + 0.5) - 0.5) - duty * 0.5;
float pulse = 1.0 - smoothstep(-aaPhase, aaPhase, pd);
pulse = lerp(pulse, duty, saturate((aaPhase - 0.15) / 0.35));
float across = abs(UV.y - 0.5) * 2.0;
float aa = max(fwidth(across), 0.001);
float fill = clamp(StrokeFill, 0.1, 0.95);
float corePulse = 1.0 - smoothstep(fill-aa, fill+aa, across);
float coreFilament = 1.0 - smoothstep(fill*FilamentWidth-aa, fill*FilamentWidth+aa, across);
float outlinePulse = 1.0 - smoothstep(1.0-aa, 1.0+aa, across);
float filamentBorder = min(1.0, fill*FilamentWidth + (1.0-fill));
float outlineFilament = 1.0 - smoothstep(filamentBorder-aa, filamentBorder+aa, across);
float foreground = saturate(max(FilamentAlpha*coreFilament, PulseAlpha*pulse*corePulse));
float background = saturate(OutlineAlpha * max(outlineFilament, pulse*outlinePulse));
float coverage = foreground + background*(1.0-foreground);
float3 premultiplied = Color.rgb*Brightness*foreground + OutlineColor.rgb*background*(1.0-foreground);
return float4(premultiplied/max(coverage,0.0001), saturate(coverage*Opacity));
