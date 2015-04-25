#include "ShaderDefines.h"

Texture2D m_colorMap : register(t0);

SamplerState m_colorSampler : register(s0);

struct PixelShaderOutput
{
  float4 col : COLOR_0;
  float4 pos : COLOR_1;
  float4 norm : COLOR_2;
};

cbuffer Material : register(b0)
{
   float4 ambient;
   float4 diffuse;
   float4 specular;
   float shininess;
};

PixelShaderOutput main( PixelShaderInput input ) : SV_TARGET
{
   PixelShaderOutput output;
	output.col = m_colorMap.Sample(m_colorSampler, input.tex0);
   output.pos = float4(input.worldPos, 1.0);
   output.norm = input.norm;
   
   return output;
}

PixelShaderOutput difMain( PixelShaderInput input ) : SV_TARGET
{
   PixelShaderOutput output;
	output.col = diffuse;
   output.pos = float4(input.worldPos, 1.0);
   output.norm = input.norm;
   
   return output;
}