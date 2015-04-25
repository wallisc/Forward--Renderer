#include "SharedDefines.h"
#include "ShaderDefines.h"

Texture2D m_RWDepthBuffer : register(t0);
Texture2D m_ColorMap : register(t1);
Texture2D m_ShadowWorldPosMap : register(t2);
Texture2D m_ShadowWorldNormMap : register(t3);

AppendStructuredBuffer<PointLight> m_LightBuffer : register(u1);

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
   float4   left;
   float4   eye;
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


[numthreads(THREAD_GROUP_WIDTH, THREAD_GROUP_HEIGHT, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
   uint2 coord = uint2(DTid.x * LIGHTS_PER_X_PIXEL, DTid.y * LIGHTS_PER_Y_PIXEL);
   if(coord.x >= uint(SHADOW_WIDTH) || coord.y >= uint(SHADOW_HEIGHT)) 
      return;

   PointLight p;
   p.pos = m_ShadowWorldPosMap[coord];

   p.col = m_ColorMap[coord];
      
   p.screenPos = HomogenousDivide(mul(mvpMat, p.pos));
   float4 offsetPos = HomogenousDivide(mul(mvpMat, p.pos + left * MAX_LIGHT_RADIUS));
   float4 eyeOffsetPos = HomogenousDivide(mul(mvpMat, p.pos + eye * MAX_LIGHT_RADIUS));
      
   p.screenPos = NormalizedDeviceCoordToScreen(p.screenPos);
   offsetPos = NormalizedDeviceCoordToScreen(offsetPos);
   eyeOffsetPos = NormalizedDeviceCoordToScreen(eyeOffsetPos);

   p.screenRad = float4(0, 0, 0, 0);
   p.screenRad.x = abs(length(p.screenPos.xy - offsetPos.xy));
   p.screenRad.y = abs(length(p.screenPos.z - eyeOffsetPos.z));

   m_LightBuffer.Append(p);
}