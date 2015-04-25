#include "SharedDefines.h"
#include "ShaderDefines.h"

struct VertexOutput
{
   float4 pos : SV_POSITION;
   float2 tex : TEXCOORD0;
};

#define BLUR_COUNT_SQRT 16
#define BLUR_COUNT (BLUR_COUNT_SQRT * BLUR_COUNT_SQRT) 
#define PI 3.14
#define E  2.72
#define STANDARD_DEVIATION 5.0
#define LUMA_CUTOFF 0.45

float calculateLuma(float3 color)
{
   //return color.r * .33 + color.g * 0.5 + color.b * 0.16;
   return max(max(color.r, color.g), color.b);
}

Texture2D<float4> m_source : register(t0);

SamplerState m_blackBorder : register(s2);

float4 main(VertexOutput input) : SV_TARGET
{
   float4 color = float4(0,0,0,0);
   uint numSamples = 0;

   for (int x = -(BLUR_COUNT_SQRT / 2); x < BLUR_COUNT_SQRT / 2 + 1; x++)
   {
      for (int y = -(BLUR_COUNT_SQRT / 2); y < BLUR_COUNT_SQRT / 2 + 1; y++)
      {
         float2 dx = float2(x / (WIDTH / BLOOM_DIVISION), y / (HEIGHT / BLOOM_DIVISION));
         float4 sampleColor = m_source.Sample(m_blackBorder, input.tex + dx);
         float luma = calculateLuma(sampleColor.xyz);
         uint applyColor = luma > LUMA_CUTOFF;
         float factor = (1.0f / (STANDARD_DEVIATION * STANDARD_DEVIATION * 2 * PI)) * pow(E, - (x * x + y * y) / (2 * STANDARD_DEVIATION * STANDARD_DEVIATION ));
         color += sampleColor * applyColor * factor;
      }
   }
   color *= 1.0f;

   return color;
}