struct VSOutput
{
    float4 Position : SV_Position;
    float3 Color : COLOR0;
    float2 Uv : TEXCOORD0;
};

sampler LinearSampler : register(s0);

Texture2D Texture : register(t0);

float4 Main(VSOutput input) : SV_Target
{
    float4 texel = Texture.Sample(LinearSampler, input.Uv);
    float4 col = float4(input.Color, 1.0f) + texel;
    col = min(float4(1.0, 1.0, 1.0, 1.0), col);
    return texel;
}