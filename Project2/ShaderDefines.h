struct PointLight
{
   float4 pos;
   float4 col;
   float4 screenPos;
   float4 screenRad;
};

struct VertexShaderInput
{
  float4 pos : POSITION;
  float2 tex0 : TEXCOORD0;
  float4 norm : NORMAL0;
  float4 tang : NORMAL1;
  float4 bitang: NORMAL2;
};

// TODO: (msft-Chris) Revisit this struct and slim out variables that aren't needed
struct PixelShaderInput
{
  float4 pos : SV_POSITION;
  float3 worldPos : POSITIONT;
  float2 tex0: TEXCOORD0;
  float4 norm : NORMAL0;
  float4 lPos : TEXCOORD1;
  float4 tang : NORMAL1;
  float4 bitang : NORMAL2;
  float2 linearDepth : TEXCOORD2;
};

#define EPSILON 0.00001f

