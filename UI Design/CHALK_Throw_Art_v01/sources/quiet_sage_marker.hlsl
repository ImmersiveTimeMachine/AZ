// Native GIMP masks: R outer corners, G incomplete ellipse, B center oval.
float outer = saturate(Masks.r * CornerAlpha);
float inner = saturate(Masks.g * RingAlpha);
float center = saturate(Masks.b * CenterAlpha);
float whiteCoverage = max(inner, center);
float coverage = max(outer, whiteCoverage);
float3 tint = (Color.rgb * outer + CenterColor.rgb * whiteCoverage) / max(outer + whiteCoverage, 0.0001);
return float4(tint * Brightness, saturate(coverage * Opacity));
