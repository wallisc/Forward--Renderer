#include "SharedDefines.h"
#include "ShaderDefines.h"

cbuffer ConstBuffer : register(b1)
{
   float4x4 mvpMat;
   float4x4 invTrans;
   float4x4 mvMat;
   float4x4 proj;
};

StructuredBuffer<PointLight> m_Lights : register(t0);
Texture2D<float4> m_DepthBuffer : register(t1);

RWStructuredBuffer<uint>  m_LightCount : register(u0);
RWStructuredBuffer<uint>  m_TiledLights : register(u1);

[numthreads(THREAD_GROUP_WIDTH, THREAD_GROUP_HEIGHT, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
   float px1 = DTid.x * TILE_WIDTH;
   float py1 = DTid.y * TILE_HEIGHT;
   float px2 = px1 + TILE_WIDTH;
   float py2 = py1 + TILE_HEIGHT;

   uint tileWidth = TILE_WIDTH;
   uint tileHeight = TILE_HEIGHT;
   float minDepth = 1.0f;
   float maxDepth = 0.0f;
   for(uint i = 0; i < tileWidth; i++)
   {
      for(uint j = 0; j < tileHeight; j++)
      {
         uint2 coord = uint2(px1 + i, py1 + j);
         float4 depth = m_DepthBuffer[coord];
         minDepth = min(minDepth, depth.r);
         maxDepth = max(maxDepth, depth.r);
      }
   }
   
   uint index = DTid.y * NUM_X_TILES + DTid.x;
   uint offset = index * MAX_LIGHTS_PER_TILE;
   uint lightCount = 0;

   for(uint i = 0; i < NUM_LIGHTS; i++)
   {
      float radius = m_Lights[i].screenRad.r;
      float zRadius = m_Lights[i].screenRad.g;
      float x1 = m_Lights[i].screenPos.x - radius;
      float x2 = m_Lights[i].screenPos.x + radius;
      float y1 = m_Lights[i].screenPos.y - radius;
      float y2 = m_Lights[i].screenPos.y + radius;
      
	  if (x1 < px2 && x2 > px1 && y1< py2 && y2 > py1 && m_Lights[i].screenPos.z + zRadius > minDepth && m_Lights[i].screenPos.z - zRadius < maxDepth)
      {
         m_TiledLights[offset + lightCount] = i;
         lightCount++;
      }
   }
   m_LightCount[index] = lightCount;
}