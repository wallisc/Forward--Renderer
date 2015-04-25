#include "SharedDefines.h"
#include "ShaderDefines.h"

struct VertexOutput
{
   float4 pos : SV_POSITION;
   float2 tex : TEXCOORD0;
};

Texture2D<float4> m_source1 : register(t0);
Texture2D<float4> m_source2 : register(t1);

SamplerState m_colorSampler : register(s0);

float4 main(VertexOutput input) : SV_TARGET
{
   return m_source1.Sample(m_colorSampler, input.tex) + m_source2.Sample(m_colorSampler, input.tex);
}