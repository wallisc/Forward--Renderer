#include "SharedDefines.h"
#include "ShaderDefines.h"

Texture2D m_RWDepthBuffer : register(t0);
Texture2D m_ColorMap : register(t1);
Texture2D m_ShadowWorldPosMap : register(t2);
Texture2D m_ShadowWorldNormMap : register(t3);



RWTexture2D<float4>  m_BlurredMap;
AppendStructuredBuffer<PointLight> m_LightBuffer;

cbuffer Lights : register(b0)
{
   float4 lightDir;
   float4x4 lightMvp;
   float4x4 lightInvMvp;
};

cbuffer ConstBuffer : register(b1)
{
   float4x4 mvpMat;
   float4x4 invTrans;
   float4x4 mvMat;
   float4x4 proj;
};

float4 HomogenousDivide(float4 input)
{
   float4 output;
   output.xyz = input.xyz / input.w;
   output.w = 1.0;
   return output;
}

float4 NormalizedDeviceCoordToScreen(float4 input)
{
   float4 output = input;
   output.x = (output.x / 2.0f + 0.5f) * WIDTH;
   output.y = (output.y / -2.0f + 0.5f) * HEIGHT;
   return output;
}

#define BLUR_BLOCK_LENGTH 1
#define NUM_SAMPLES BLUR_BLOCK_LENGTH * BLUR_BLOCK_LENGTH

[numthreads(THREAD_GROUP_WIDTH, THREAD_GROUP_HEIGHT, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
   float4 blurColor = float4(0, 0, 0, 0);
   for( int x = -BLUR_BLOCK_LENGTH / 2; x < BLUR_BLOCK_LENGTH / 2 + 1; x++ )
   {
      for( int y = -BLUR_BLOCK_LENGTH / 2; y < BLUR_BLOCK_LENGTH / 2 + 1; y++ )
      {
         x = clamp(x, 0, SHADOW_WIDTH - 1);
         y = clamp(y, 0, SHADOW_HEIGHT - 1);
         blurColor += m_RWDepthBuffer[DTid.xy + int2(x, y)] / float(NUM_SAMPLES);
      }
   }
   m_BlurredMap[DTid.xy] = blurColor;
}