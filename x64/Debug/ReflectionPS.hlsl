#include "SharedDefines.h"
#include "ShaderDefines.h"

struct VertexOutput
{
   float4 pos : SV_POSITION;
   float2 tex : TEXCOORD0;
};

Texture2D<float4> m_Image : register(t0);
Texture2D<float4> m_worldPos : register(t1);
Texture2D<float4> m_worldRefl : register(t2);
Texture2D<float4> m_screenRefl : register(t3);
Texture2D<float4> m_depthBuffer : register(t4);

#define INTERSECT_RADIUS 30.0000f

float SphereIntersect(float3 l, float3 o, float3 c, float r)
{
   return dot(l, o -c) * dot(l, o -c) - length(o-c) * length(o-c) + r * r;
}

float4 main(VertexOutput input) : SV_TARGET
{
   int2 origin = input.tex * float2(WIDTH, HEIGHT);
   float3 dir = m_screenRefl[uint2(origin)].xyz;
   
   float3 reflectionRay = m_worldRefl[uint2(origin)].xyz;
   float3 reflectionOrigin = m_worldPos[uint2(origin)].xyz;
   float baseDepth = m_depthBuffer[origin].r;

   int2 coord;
   float t = 2;
   float reflectionFound = 0.0f;
   while( t < 1000)
   {
      coord = int2(origin + dir * t);
      if(coord.x >= int(WIDTH) || coord.y >= int(HEIGHT)) break;

      float3 pos = m_worldPos[coord].xyz;
#if 0
      if( SphereIntersect(reflectionRay, reflectionOrigin, pos, INTERSECT_RADIUS) >= 0.0f)
#else
      if( baseDepth + (t * dir).z + EPSILON <= m_depthBuffer[coord].r)
#endif
      {
         reflectionFound = 1.0f;
         break;
      }
      t += 1.0f;
   }
  return float4(baseDepth, 0, 0, 1);
 
  return m_Image[int2(origin)] * .1f + m_Image[coord] * .9f* reflectionFound;
  //return float4(dir.z * 4000.0f, 0, 0, 1);
   //return float4(reflectionRay, 1);
}