#include "SharedDefines.h"
#include "ShaderDefines.h"

Texture2D m_colorMap : register(t0);
Texture2D m_bumpMap : register(t1);

Texture2D m_RWDepthBuffer : register(t2);
StructuredBuffer<PointLight> m_lightBuffer : register(t3);
StructuredBuffer<uint> m_lightIndexBuffer : register(t4);
StructuredBuffer<uint> m_numLights : register(t5);

SamplerState m_colorSampler : register(s0);
SamplerState m_shadowSampler : register(s1);

cbuffer Material : register(b0)
{
   float4 ambient;
   float4 diffuse;
   float4 specular;
   float shininess;
};

cbuffer Lights : register(b1)
{
   float4 lightDir;
   float4x4 lightMvp;
   float4x4 lightInvMvp;
};

cbuffer ConstBuffer : register(b2)
{
   float4x4 mvpMat;
   float4x4 invTrans;
   float4x4 mvMat;
   float4x4 proj;
   float4 left;
   float4 eye;
   float4 camPos;
};

struct PixelShaderOutput
{
  float4 worldPos : COLOR_0;
  float4 refl : COLOR_1;
  float4 screenRefl: COLOR_2;
  float4 screenPos: COLOR_3;
};

PixelShaderOutput main( PixelShaderInput input ) : SV_TARGET
{
    PixelShaderOutput output;
    output.worldPos = float4(input.worldPos, 1);

    float4 d = float4(normalize(output.worldPos.xyz - camPos.xyz), 0);
    float4 n = float4(normalize(input.norm.xyz), 0);
    output.refl = reflect(d, n);
    output.refl.w = 0.0f;
#if 0
    output.screenRefl = float4(normalize(mul(mvpMat, output.refl).xy), 0, 0);
    output.screenRefl.y = -output.screenRefl.y;
#else
    float4 screenRefl= mul(mvpMat, output.refl);
    float divisor = length(screenRefl.xy);
    output.screenRefl = screenRefl / divisor;
    output.screenRefl.y = -output.screenRefl.y;
    output.screenRefl.z = -output.screenRefl.z / 4000.0f;
    //output.screenRefl.z = 1.0f - output.screenRefl.z;
    output.screenPos = float4(0, 0, 0, 0);

#endif
    return output;
}