#include "SharedDefines.h"
#include "ShaderDefines.h"

struct VertexOutput
{
   float4 pos : SV_POSITION;
   float2 tex : TEXCOORD0;
};

Texture2D<float4> m_Image : register(t0);
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
   
   float baseDepth = m_depthBuffer[origin].r;

   int2 coord;
   float t = 1;
   float reflectionFound = 0.0f;
   while( t < 1000)
   {
      coord = int2(origin + dir * t);
      if(coord.x >= int(WIDTH) || coord.y >= int(HEIGHT)) break;


      if( baseDepth + t * dir.z - EPSILON > m_depthBuffer[coord].r)
      {
         reflectionFound = 1.0f;
         break;
      }
      t += 1.0f;
   }
 
   return m_Image[int2(origin)] * .0f + m_Image[coord] * .8f * reflectionFound;
}