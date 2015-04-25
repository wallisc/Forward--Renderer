#include "ShaderDefines.h"

cbuffer ConstBuffer
{
   float4x4 mvpMat;
   float4x4 invTrans;
   float4x4 mvMat;
   float4x4 proj;
};

cbuffer Lights 
{
   float4 lightDir;
   float4x4 lightMvp;
};

PixelShaderInput main( VertexShaderInput input )
{
    PixelShaderInput output;
    output.pos = mul(mvpMat, input.pos);
    output.worldPos = input.pos.xyz;
    output.norm = input.norm;
    output.lPos = mul(lightMvp, input.pos);
    output.tex0 = input.tex0;
    output.tang = input.tang;
    output.bitang = input.bitang;

    return output;
}