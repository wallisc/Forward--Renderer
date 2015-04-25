struct VertexOutput
{
   float4 pos : SV_POSITION;
   float2 tex : TEXCOORD0;
};

Texture2D<float4> m_source : register(t0);
SamplerState m_colorSampler : register(s0);

float4 main(VertexOutput input) : SV_TARGET
{
   return m_source.Sample(m_colorSampler, input.tex);

}