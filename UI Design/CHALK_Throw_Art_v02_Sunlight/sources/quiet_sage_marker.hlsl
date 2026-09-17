// Keep the original editable R/G/B masks. Dilate their union for a charcoal keyline.
float3 masks = Texture2DSample(MaskTex, MaskTexSampler, UV).rgb;
float outer = saturate(masks.r*CornerAlpha);
float whiteCoverage = saturate(max(masks.g*RingAlpha,masks.b*CenterAlpha));
float foreground = max(outer,whiteCoverage);
float3 foregroundTint = (Color.rgb*outer + CenterColor.rgb*whiteCoverage)/max(outer+whiteCoverage,0.0001);
float2 pixelUV = max(OutlinePixels,0.0)/max(float2(MaskCanvasWidth,MaskCanvasHeight),float2(1,1));
float dilation = max(masks.r,max(masks.g,masks.b));
[unroll] for(int i=0;i<8;i++)
{
    float angle = i*0.78539816339;
    float2 delta = float2(cos(angle),sin(angle))*pixelUV;
    float3 sampleMask = Texture2DSample(MaskTex,MaskTexSampler,UV+delta).rgb;
    dilation = max(dilation,max(sampleMask.r,max(sampleMask.g,sampleMask.b)));
}
float background = saturate(dilation*OutlineAlpha);
float coverage = foreground + background*(1.0-foreground);
float3 premultiplied = foregroundTint*Brightness*foreground + OutlineColor.rgb*background*(1.0-foreground);
return float4(premultiplied/max(coverage,0.0001),saturate(coverage*Opacity));
