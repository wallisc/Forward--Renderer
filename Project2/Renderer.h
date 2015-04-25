#ifndef _BLANK_DEMO_H_
#define _BLANK_DEMO_H_

#include "D3DBase.h"
#include "Mesh.h"
#include "Material.h"


#include "RWDepthBuffer.h"
#include "ConstantBuffer.h"
#include "RWRenderTarget.h"
#include "RWComputeSurface.h"
#include "RWStructuredBuffer.h"
#include "PlaneRenderer.h"
#include "Camera.h"
#include "FW1FontWrapper.h"

#include <assimp/scene.h>           // Output data structure

#include <xnamath.h>
#include <vector>
#include <map>

#define MAX_COLOR_BUFFER_DEPTH 8

__declspec(align(16))
struct PS_Point_Light
{
   XMFLOAT4 position;
   XMFLOAT4 color;
   XMFLOAT4 screenPosition;
   XMFLOAT4 screenRadius;
};

struct VS_Transformation_Constant_Buffer 
{
   XMMATRIX mvp;
   XMMATRIX invTrans;
   XMMATRIX mv;
   XMMATRIX proj;
   XMFLOAT4 left;
   XMFLOAT4 eye;
   XMFLOAT4 camPos;
};

__declspec(align(16))
struct PS_Light_Constant_Buffer
{
   XMFLOAT4 direction;
   XMFLOAT4X4 mvp;
   XMFLOAT4X4 invMvp;
};

__declspec(align(16))
struct PS_Material_Constant_Buffer
{
   XMFLOAT4 ambient;
   XMFLOAT4 diffuse;
   XMFLOAT4 specular;
   FLOAT shininess;
};

enum RenderPass
{
   ShadowPass = 0,
   DepthPrePass,
   MainRender,
   NumPasses
};

class Renderer : public D3DBase
{
public:
   Renderer();
   void Update(FLOAT dt, BOOL *keyInputArray);
   void Render();
   bool LoadContent();
   void UnloadContent();

private:
   bool InitializeMatMap(const aiScene *pAssimpScene);
   void DestroyMatMap();

   bool CreateD3DMesh(const aiMesh *pMesh, const aiScene *pAssimpScene, Mesh *d3dMesh);

   void DestroyD3DMesh(Mesh *d3dMesh);

   UINT m_ShadowMapHeight;
   UINT m_ShadowMapWidth;

   D3D11_VIEWPORT m_viewport;
   D3D11_VIEWPORT m_bloomViewport;

   ID3D11VertexShader* m_solidColorVS;
   ID3D11VertexShader* m_planeVS;

   ID3D11PixelShader* m_solidColorPS;
   ID3D11PixelShader* m_prePassPS;
   ID3D11PixelShader* m_texturePS;
   ID3D11PixelShader* m_textureNoShadingPS;
   ID3D11PixelShader* m_diffuseNoShadingPS;
   ID3D11PixelShader* m_bloomPS;
   ID3D11PixelShader* m_blendPS;
   ID3D11PixelShader* m_reflectionPS;
   ID3D11PixelShader* m_bumpTexturePS;

   ID3D11InputLayout* m_inputLayout;

   ConstantBuffer<VS_Transformation_Constant_Buffer> *m_pTransformConstants;
   ConstantBuffer<PS_Light_Constant_Buffer> *m_pLightConstants;

   RWRenderTarget* m_pShadowMapWorldPos;
   RWRenderTarget* m_pShadowMapWorldNorm;

   RWRenderTarget* m_pBlurredShadowMap;
   RWRenderTarget* m_pLightMap;
   RWRenderTarget* m_pPostProcessRTV;
   RWRenderTarget* m_pBrightMap;

   RWRenderTarget* m_pReflectionRTV;
   RWRenderTarget* m_pWorldPosMap;
   RWRenderTarget* m_pWorldRefMap;
   RWRenderTarget* m_pScreenReflMap;
   RWRenderTarget* m_pLinearDepthMap;

   ID3D11ComputeShader* m_lightCullingCS;

   RWStructuredBuffer<PS_Point_Light> *m_pLightBuffer;
   RWStructuredBuffer<UINT> *m_pTileLightIndexBuffer;
   RWStructuredBuffer<UINT> *m_pLightsPerTileBuffer;

   UINT m_tileXCount;
   UINT m_tileYCount;

   ID3D11ComputeShader* m_lightGeneratorCS;
   ID3D11ComputeShader* m_blurCS;
   RWComputeSurface* m_pBlurredShadowSurface;

   PlaneRenderer* m_pPlaneRenderer;

   ID3D11SamplerState* m_colorMapSampler;
   ID3D11SamplerState* m_shadowSampler;
   ID3D11SamplerState* m_blackBorderSampler;

   ID3D11RasterizerState* m_rasterState;
   ID3D11DepthStencilState* m_depthState;

   std::vector<Mesh> scene;
   std::vector<Material> m_matList;

   RWDepthBuffer *m_pRWDepthBuffer;
   RWDepthBuffer *m_pPrePassDepthStencil;

   // At some point these should be encapsulated into a Mesh Object 
   VS_Transformation_Constant_Buffer m_vsTransConstBuf;
   VS_Transformation_Constant_Buffer m_vsLightTransConstBuf;

   PS_Material_Constant_Buffer m_psMaterialConstBuf;
   PS_Light_Constant_Buffer m_psLightConstBuf;


   XMMATRIX m_camViewTrans;

   XMFLOAT4 m_lightDirection;
   XMFLOAT4 m_lightUp;

   Camera *m_pCamera;

   IFW1Factory *m_pFW1Factory;
   IFW1FontWrapper *m_pFontWrapper;

   float m_cameraUnit;

   // TODO: Encapsulate in a perspective object
   float m_nearPlane;
   float m_farPlane;
   float m_fieldOfView; // vertical FOV in radians

   double m_fps;
};

#endif //_BLANK_DEMO_H_