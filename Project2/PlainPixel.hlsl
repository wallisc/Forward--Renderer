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
SamplerState m_blackBorderSampler : register(s2);

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

// TODO: Pass in via constant buffer
#define SHADOW_SAMPLES_SQRT 3
#define SHADOW_SAMPLES (SHADOW_SAMPLES_SQRT * SHADOW_SAMPLES_SQRT)
#define BUMP_SCALE 2.0

float3 phong( float3 norm, float3 eye, float3 lightDir, float3 lightColor, float3 amb, float3 dif, float3 spec, float shininess)
{
    //float3 h = normalize(2.0 * norm - lightDir);
    float nDotL = clamp(dot(norm, lightDir), 0.0, 1.0);

    return  dif * nDotL * lightColor;
}

float3 phong_onlyDiffuse( float3 norm, float3 eye, float3 lightDir, float3 lightColor, float3 dif)
{
   return phong(norm, eye, lightDir, lightColor, float3(0,0,0), dif, float3(0,0,0), 0);
}

float4 texShader( PixelShaderInput input , float3 n, float3 dif )
{
    float3 e = -normalize(input.pos.xyz);
    float3 lightClr = float3(1, 1, 1) * 0.7;

    float3 spec = specular.xyz;
    float3 amb = dif.xyz * .1f;
    
    float3 color = amb;
    float depthValue = 1.0;
    input.lPos.x = input.lPos.x / 2.0 + 0.5;
    input.lPos.y = input.lPos.y / -2.0 + 0.5;
    
    uint tileX = input.pos.x / TILE_WIDTH;
    uint tileY = input.pos.y / TILE_HEIGHT;
    uint index = tileY * NUM_X_TILES + tileX;
    uint numLights = m_numLights[index];
    for(uint light = 0; light < numLights; light++)
    {
       uint offset = index * MAX_LIGHTS_PER_TILE;
       uint lightIndex = m_lightIndexBuffer[light + offset];
       float dist = distance(input.worldPos.xyz,  m_lightBuffer[lightIndex].pos.xyz);
       float3 pointLightDir = (m_lightBuffer[lightIndex].pos.xyz - input.worldPos.xyz) * rcp( dist );
       
       float lightFactor = (MAX_LIGHT_RADIUS - dist) * rcp( MAX_LIGHT_RADIUS );
       lightFactor = clamp(lightFactor, 0.0f, 0.2f) * LIGHT_POWER; // Prevents hot spots from the point lights
       color += (lightFactor * phong_onlyDiffuse(n, e, pointLightDir, m_lightBuffer[lightIndex].col, dif));
    }

    int samplesShadowed = SHADOW_SAMPLES;
    for (int x = -(SHADOW_SAMPLES_SQRT / 2); x < SHADOW_SAMPLES_SQRT / 2 + 1; x++)
    {
      for (int y = -(SHADOW_SAMPLES_SQRT / 2); y < SHADOW_SAMPLES_SQRT / 2 + 1; y++)
      {
         float3 dx = float3( float(x) / SHADOW_WIDTH, float(y) / SHADOW_HEIGHT, 0.0);
         float3 samplePt = input.lPos.xyz + dx;

         if ( samplePt.z > 0.1 && samplePt.z < 1)
         {
            depthValue = m_RWDepthBuffer.Sample(m_blackBorderSampler, samplePt.xy).r;
            if ( input.lPos.z - EPSILON <= depthValue )
            {
              samplesShadowed--;
            }
         }
      }
    }

    if (samplesShadowed < SHADOW_SAMPLES)
    {
       float shadowFactor = float(samplesShadowed) / float(SHADOW_SAMPLES);
       //color += phong(n, e, lightDir, lightClr, float3(0.0, 0.0, 0.0), dif, spec, shininess) * (1.0f - shadowFactor);
       color += phong(n, e, lightDir, lightClr, float3(0.0, 0.0, 0.0), dif, spec, shininess);
    }

    return float4(color, 1.0f);
}

float4 main( PixelShaderInput input ) : SV_TARGET
{
    float3 n = normalize(input.norm.xyz);
    return texShader( input, n, diffuse.rgb );
}

float4 bumpMain( PixelShaderInput input ) : SV_TARGET
{
   float2 bump2D = BUMP_SCALE * (m_bumpMap.Sample(m_colorSampler, input.tex0).xy * 2.0 - 1.0);
   float z = sqrt(1.0 - dot(bump2D.xy, bump2D.xy));
   float3 bumpNorm = float3(bump2D, z);

   float3 normal = normalize(bumpNorm.x * input.tang + bumpNorm.y * input.bitang + bumpNorm.z * input.norm);
   float4 texColor = m_colorMap.Sample(m_colorSampler, input.tex0);
   return texShader(input, normal, texColor.xyz);
}

float4 texMain( PixelShaderInput input ) : SV_TARGET
{
   float4 texColor = m_colorMap.Sample(m_colorSampler, input.tex0);

   float3 n = normalize(input.norm.xyz);
   return texShader(input, n, texColor.xyz);
}

