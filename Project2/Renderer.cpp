#include "Renderer.h"
#include "D3DUtils.h"

#include <cassert>
#include <string>
#include <time.h>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/postprocess.h>     // Post processing flags

#if 0
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"
using namespace tbb;
#endif 
#include "SharedDefines.h"


#if 0
class Derp {
public: 
  void operator() ( const blocked_range<size_t>& r ) const {}
};
#endif

#define CEIL_DIV(a,b) ((a - 1 ) / b + 1)

using std::vector;
using std::string;

const XMFLOAT4 LIGHT_DIRECTION(0.0f, 1.0f, 0.0f, 0.0f);
const XMFLOAT4 LIGHT_UP(0.0f, 0.0f, 1.0f, 0.0f);

const UINT TILED_WIDTH_DIVISIONS = 64;
const UINT TILED_HEIGHT_DIVISIONS = 64;

BOOL g_shadowView = false;

#define REFLECTIONS_ON 0

static double g_PCFreq;

struct VertexPos 
{
   XMFLOAT4 pos;
   XMFLOAT2 tex0;
   XMFLOAT4 norm;
   XMFLOAT4 tangent;
   XMFLOAT4 bitangent;
};

Renderer::Renderer() : D3DBase()
{

}

bool Renderer::InitializeMatMap(const aiScene *pAssimpScene)
{
   m_matList.clear();
   for( UINT i = 0; i < pAssimpScene->mNumMaterials; i++ ) 
   {
      aiMaterial *pMat = pAssimpScene->mMaterials[i];
      aiColor3D ambient, diffuse, specular;
      float shininess;

      pMat->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
      pMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
      pMat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
      pMat->Get(AI_MATKEY_SHININESS_STRENGTH, shininess);

      PS_Material_Constant_Buffer psConstBuf;
      psConstBuf.ambient = XMFLOAT4(ambient.r, ambient.g, ambient.b, 1.0f);
      psConstBuf.diffuse = XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f);
      psConstBuf.specular = XMFLOAT4(specular.r, specular.g, specular.b, 1.0f);
      psConstBuf.shininess = shininess;

      D3D11_BUFFER_DESC constBufDesc;
      ZeroMemory(&constBufDesc, sizeof( constBufDesc ));
      constBufDesc.Usage = D3D11_USAGE_DEFAULT;
      constBufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      constBufDesc.ByteWidth = sizeof( PS_Material_Constant_Buffer );

      D3D11_SUBRESOURCE_DATA constResourceData;
      ZeroMemory(&constResourceData, sizeof( constResourceData ));
      constResourceData.pSysMem = &psConstBuf;

      Material matInfo = {};
      HRESULT d3dResult = m_d3dDevice->CreateBuffer( &constBufDesc, &constResourceData, &matInfo.m_materialConstantBuffer);

      if ( FAILED(d3dResult) ) return false;

      UINT texCount = pMat->GetTextureCount(aiTextureType_DIFFUSE);
      if ( texCount > 0 )
      {
         aiString path;
         assert(texCount == 1);
         pMat->GetTexture(aiTextureType_DIFFUSE, 0, &path);
         // TODO: hack that only takes in .jpgs
         if( path.C_Str()[path.length - 1] == 'g' )
         {
            HRESULT result = D3DX11CreateShaderResourceViewFromFile(m_d3dDevice, 
                                                               path.C_Str(),
                                                               0, 
                                                               0, 
                                                               &matInfo.m_texture, 
                                                               0);
            HR(result);
         }

         UINT bumpCount = pMat->GetTextureCount(aiTextureType_HEIGHT);
         if(bumpCount > 0 )
         {
            pMat->GetTexture(aiTextureType_HEIGHT, 0, &path);

            HRESULT result = D3DX11CreateShaderResourceViewFromFile(m_d3dDevice, 
                                                   path.C_Str(),
                                                   0, 
                                                   0, 
                                                   &matInfo.m_bump, 
                                                   0);
         }
      }
      else
      {
         matInfo.m_texture = NULL;
      }
      m_matList.push_back(matInfo);
   }
   return true;
}

void Renderer::DestroyMatMap()
{
   for( UINT i = 0; i < m_matList.size(); i++ )
   {
      m_matList[i].m_materialConstantBuffer->Release();
      if( m_matList[i].m_texture )
      {
         m_matList[i].m_texture->Release();
      }
   }
}


bool Renderer::CreateD3DMesh(const aiMesh *pMesh, const aiScene *pAssimpScene, Mesh *d3dMesh)
{
   UINT numVerts = pMesh->mNumVertices;
   UINT numFaces = pMesh->mNumFaces;
   UINT numIndices = numFaces * 3;
   VertexPos *vertices = new VertexPos[numVerts]();
   UINT *indices = new UINT[numIndices]();

   assert(*pMesh->mNumUVComponents == 2 || *pMesh->mNumUVComponents == 0 );
   assert(pMesh->HasTangentsAndBitangents());
   
   memset(d3dMesh, 0, sizeof(Mesh));
   for (UINT vertIdx = 0; vertIdx < numVerts; vertIdx++)
   {
      auto pVert = &pMesh->mVertices[vertIdx];
      vertices[vertIdx].pos = XMFLOAT4(pVert->x, pVert->y, pVert->z, 1);

      auto pNorm = &pMesh->mNormals[vertIdx];
      vertices[vertIdx].norm = XMFLOAT4(pNorm->x, pNorm->y, pNorm->z, 0);
      
      auto pBitang = &pMesh->mBitangents[vertIdx];
      vertices[vertIdx].bitangent = XMFLOAT4(pBitang->x, pBitang->y, pBitang->z, 0);
 
      auto pTang = &pMesh->mTangents[vertIdx];
      vertices[vertIdx].tangent = XMFLOAT4(pTang->x, pTang->y, pTang->z, 0);

      if (*pMesh->mNumUVComponents > 0)
      {
         auto pUV = &pMesh->mTextureCoords[0][vertIdx];
         vertices[vertIdx].tex0 = XMFLOAT2(pUV->x, pUV->y);
      }
   }

   d3dMesh->m_numIndices = numIndices;
   for (UINT i = 0; i < numFaces; i++)
   {
      auto pFace = &pMesh->mFaces[i];
      assert(pFace->mNumIndices == 3);
      indices[i * 3] = pFace->mIndices[0];
      indices[i * 3 + 1] = pFace->mIndices[1];
      indices[i * 3 + 2] = pFace->mIndices[2];
   }

   auto pMat = pAssimpScene->mMaterials[pMesh->mMaterialIndex];
   aiColor3D ambient, diffuse, specular;
   float shininess;
   pMat->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
   pMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
   pMat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
   pMat->Get(AI_MATKEY_SHININESS_STRENGTH, shininess);

   // Fill in a buffer description.
   D3D11_BUFFER_DESC bufferDesc;
   bufferDesc.Usage           = D3D11_USAGE_DEFAULT;
   bufferDesc.ByteWidth       = static_cast<UINT>(sizeof( unsigned int ) * numIndices);
   bufferDesc.BindFlags       = D3D11_BIND_INDEX_BUFFER;
   bufferDesc.CPUAccessFlags  = 0;
   bufferDesc.MiscFlags       = 0;

   // Define the resource data.
   D3D11_SUBRESOURCE_DATA InitData;
   InitData.pSysMem = indices;
   InitData.SysMemPitch = 0;
   InitData.SysMemSlicePitch = 0;

   // Create the buffer with the device.
   HRESULT d3dResult = m_d3dDevice->CreateBuffer( &bufferDesc, &InitData, &d3dMesh->m_indexBuffer );
   if( FAILED( d3dResult ) ) 
   {
    	MessageBox(NULL, "CreateBuffer failed", "Error", MB_OK);
      return false;
   }
   
   D3D11_BUFFER_DESC vertexDesc;
   ZeroMemory(&vertexDesc, sizeof( vertexDesc ));
   vertexDesc.Usage = D3D11_USAGE_DEFAULT;
   vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
   vertexDesc.ByteWidth = static_cast<UINT>(sizeof( VertexPos ) * numVerts);

   D3D11_SUBRESOURCE_DATA resourceData;
   ZeroMemory(&resourceData, sizeof( resourceData ));
   resourceData.pSysMem = vertices;

   d3dResult = m_d3dDevice->CreateBuffer( &vertexDesc, &resourceData, &d3dMesh->m_vertexBuffer);

   if ( FAILED(d3dResult) ) return false;
   
   d3dMesh->m_MaterialIndex = pMesh->mMaterialIndex;

   return true;
}


void Renderer::DestroyD3DMesh(Mesh *mesh) 
{
   if( mesh->m_vertexBuffer ) mesh->m_vertexBuffer->Release();
   if( mesh->m_indexBuffer ) mesh->m_indexBuffer->Release();
}

void Renderer::Update(FLOAT dt, BOOL *keyInputArray) 
{
   static const FLOAT MOVE_SPEED = 0.001f;
   static const FLOAT ROTATE_SPEED = 0.01f;
   static const FLOAT LIGHT_ROTATE_SPEED = 0.004f;

   float tx = 0.0f, ty = 0.0f, tz = 0.0f;
   float ry = 0.0f, rx = 0.0f;
   float lightRotation = 0.0f;
   const float MoveUnit = m_cameraUnit * MOVE_SPEED;

   if( keyInputArray['P'])
   {
      g_shadowView = !g_shadowView;
   }

   if( keyInputArray['Q'])
   {
      tz += m_cameraUnit;
   }
   if( keyInputArray['E'])
   {
      tz -= m_cameraUnit; 
   }
   if( keyInputArray['W'])
   {
      ty += m_cameraUnit; 
   }
   if( keyInputArray['S'])
   {
      ty -= m_cameraUnit; 
   }
   if( keyInputArray['A'])
   {
      tx -= m_cameraUnit; 
   }
   if( keyInputArray['D'])
   {
      tx += m_cameraUnit; 
   }
   if( keyInputArray['J'])
   {
      ry -= ROTATE_SPEED; 
   }
   if( keyInputArray['L'])
   {
      ry += ROTATE_SPEED; 
   }
   if( keyInputArray['K'])
   {
      rx -= ROTATE_SPEED; 
   }
   if( keyInputArray['I'])
   {
      rx += ROTATE_SPEED; 
   }
   
   if( keyInputArray['Z'])
   {
      lightRotation += LIGHT_ROTATE_SPEED;
   }
   if( keyInputArray['C'])
   {
      lightRotation -= LIGHT_ROTATE_SPEED;
   }

   XMVECTOR xAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
   XMFLOAT3 pos(tx, ty, tz);

   m_pCamera->MoveCamera(XMLoadFloat3(&pos));
   m_pCamera->RotateCameraHorizontally(ry);
   m_pCamera->RotateCameraVertically(rx);

   XMMATRIX perspective = XMMatrixPerspectiveFovLH(m_fieldOfView, (FLOAT)m_width / (FLOAT)m_height, m_nearPlane, m_farPlane);

   XMMATRIX ortho = XMMatrixOrthographicLH(m_ShadowMapWidth, m_ShadowMapHeight, m_nearPlane, m_farPlane);

   XMMATRIX view = *m_pCamera->GetViewMatrix();
   
   XMMATRIX lightRotationMatrix = XMMatrixRotationAxis(xAxis, lightRotation);
   XMVECTOR lightDirVector = XMVector4Normalize(XMVector4Transform(XMLoadFloat4(&m_lightDirection), lightRotationMatrix));
   XMVECTOR lightUpVector =  XMVector4Normalize(XMVector4Transform(XMLoadFloat4(&m_lightUp), lightRotationMatrix));
   
   XMFLOAT4 lookAtPoint = XMFLOAT4(0, 0, 0, 0);
   XMVECTOR lookAtPointVec = XMLoadFloat4(&lookAtPoint);
   
   XMStoreFloat4(&m_lightDirection, lightDirVector);
   XMStoreFloat4(&m_lightUp, lightUpVector);

   XMVECTOR shadowEye = XMVectorSetW(lightDirVector * 2000.0f, 1.0f);
   XMMATRIX lightView = XMMatrixLookAtLH(shadowEye, lookAtPointVec, lightUpVector);

   m_vsTransConstBuf.mvp = view * perspective;
   m_vsTransConstBuf.mv = view;
   m_vsTransConstBuf.proj = perspective;
   XMStoreFloat4(&m_vsTransConstBuf.left, m_pCamera->GetLeft());
   XMStoreFloat4(&m_vsTransConstBuf.eye, m_pCamera->GetEye());
   XMStoreFloat4(&m_vsTransConstBuf.camPos, m_pCamera->GetPos());

   m_vsLightTransConstBuf.mvp = lightView * ortho;
   
   m_psLightConstBuf.direction = m_lightDirection;

   XMStoreFloat4x4(&m_psLightConstBuf.mvp, m_vsLightTransConstBuf.mvp);

   XMVECTOR determinant;
   XMMATRIX lightViewInv = XMMatrixInverse(&determinant, lightView);
   XMMATRIX orthoInv = XMMatrixInverse(&determinant, ortho);
   XMVECTOR dummy;
   m_vsTransConstBuf.invTrans = XMMatrixTranspose(XMMatrixInverse(&dummy, m_vsTransConstBuf.mvp));

   XMMATRIX invMvp = orthoInv * lightViewInv;
   XMStoreFloat4x4(&m_psLightConstBuf.invMvp, invMvp);
}

void Renderer::Render() 
{
   if (!m_d3dContext) return;

   static LARGE_INTEGER lastTime = {};
   LARGE_INTEGER curTime;

   QueryPerformanceCounter(&curTime);

   if(lastTime.QuadPart != 0)
   {
      double dt = double(curTime.QuadPart-lastTime.QuadPart)/g_PCFreq;
      m_fps = 1.0 / (dt / 1000.0);
   }
   lastTime = curTime;

   float clearColor[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
   float clearNormals[4] = { 0.5f, 0.5f, 0.5f, 0.0f };
   float clearDepth[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

   unsigned int zeroUints[4] = { 0, 0, 0, 0};

   m_d3dContext->ClearRenderTargetView(m_pPostProcessRTV->GetRenderTargetView(), clearColor);
   
   m_d3dContext->ClearUnorderedAccessViewFloat(m_pBlurredShadowSurface->GetUnorderedAccessView(), clearDepth);
   m_d3dContext->ClearUnorderedAccessViewUint(m_pLightsPerTileBuffer->GetUnorderedAccessView(), zeroUints);

   // Clear the depth buffer to 1.0f and the stencil buffer to 0.
   m_d3dContext->ClearDepthStencilView(m_pPrePassDepthStencil->GetDepthStencilView(),
     D3D11_CLEAR_DEPTH, 1.0f, 0);

   m_d3dContext->ClearDepthStencilView(m_pRWDepthBuffer->GetDepthStencilView(),
     D3D11_CLEAR_DEPTH, 1.0f, 0);

   unsigned int stride = sizeof(VertexPos);
   unsigned int offset= 0;


   m_d3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
   m_d3dContext->RSSetState(m_rasterState);
   m_d3dContext->OMSetDepthStencilState(m_depthState, 0);

   m_pLightConstants->SetData(m_d3dContext, &m_psLightConstBuf);
   ID3D11Buffer *pFirstPassCbs[] = { m_pLightConstants->GetConstantBuffer(), m_pTransformConstants->GetConstantBuffer() };
   m_d3dContext->PSSetConstantBuffers(1 , 2, pFirstPassCbs);
   m_d3dContext->VSSetConstantBuffers(1 , 1, pFirstPassCbs);


   ID3D11SamplerState *samplers[] = { m_colorMapSampler, m_shadowSampler, m_blackBorderSampler };
   m_d3dContext->PSSetSamplers(0 , ARRAYSIZE(samplers), samplers);
         
   m_d3dContext->PSSetShader(m_solidColorPS, 0, 0);
   ID3D11Buffer *pCbs[] = { m_pTransformConstants->GetConstantBuffer() };
   m_d3dContext->VSSetConstantBuffers(0 , 1, pCbs);

   for (UINT draw = 0; draw < NumPasses; draw++)
   {
      if (draw != ShadowPass && g_shadowView) break;

      if (draw == ShadowPass)
      {
         m_d3dContext->IASetInputLayout( m_inputLayout );
         m_d3dContext->RSSetViewports(1, m_pRWDepthBuffer->GetViewport());
         m_d3dContext->VSSetShader(m_solidColorVS, 0, 0);
       
         m_pTransformConstants->SetData(m_d3dContext, &m_vsLightTransConstBuf);
         
         ID3D11RenderTargetView *pLightMapRtv[] = { 
            m_pLightMap->GetRenderTargetView(),
            m_pShadowMapWorldPos->GetRenderTargetView(),
            m_pShadowMapWorldNorm->GetRenderTargetView()
         };

         if(g_shadowView) 
         {
            pLightMapRtv[0] = m_backBufferTarget;

         }

         ID3D11ShaderResourceView *pNullSrv[] = { NULL };

         m_d3dContext->PSSetShaderResources(1 , 1, pNullSrv);
         m_d3dContext->OMSetRenderTargets(ARRAYSIZE(pLightMapRtv), pLightMapRtv, m_pRWDepthBuffer->GetDepthStencilView());
      }
      else if (draw == DepthPrePass)
      {
         ID3D11RenderTargetView *pPrepassRtv[] = { 
            m_pWorldPosMap->GetRenderTargetView(),
            m_pWorldRefMap->GetRenderTargetView(),
            m_pScreenReflMap->GetRenderTargetView(),
            m_pLinearDepthMap->GetRenderTargetView(),
         };
         m_d3dContext->OMSetRenderTargets(ARRAYSIZE(pPrepassRtv), pPrepassRtv, m_pPrePassDepthStencil->GetDepthStencilView());
         m_d3dContext->RSSetViewports(1, &m_viewport);
         m_d3dContext->IASetInputLayout( m_inputLayout );
         m_d3dContext->VSSetShader(m_solidColorVS, 0, 0);

         m_pTransformConstants->SetData(m_d3dContext, &m_vsTransConstBuf);
      }
      else if (draw == MainRender)
      {
         ID3D11UnorderedAccessView *pNullUav[] = { NULL, NULL };
         ID3D11ShaderResourceView *pNullSrv[] = { NULL, NULL, NULL, NULL };
         ID3D11RenderTargetView *pNullRtv[] = { NULL, NULL };

         m_d3dContext->OMSetRenderTargets(2, pNullRtv, NULL);
         m_d3dContext->PSSetShaderResources(0 , ARRAYSIZE(pNullSrv), pNullSrv);


         ID3D11ShaderResourceView *pSrv[] = { 
            m_pRWDepthBuffer->GetShaderResourceView(),
            m_pLightMap->GetShaderResourceView(),
            m_pShadowMapWorldPos->GetShaderResourceView(),
            m_pShadowMapWorldNorm->GetShaderResourceView()
         };
         ID3D11UnorderedAccessView *pUav[] = { 
            m_pBlurredShadowSurface->GetUnorderedAccessView(),
            m_pLightBuffer->GetUnorderedAccessView() 
         };

         UINT initialCounts[] = {
            0,
            0 };

         m_d3dContext->CSSetShader(m_blurCS, NULL, 0);
         ID3D11Buffer *pBlurCsCbs[] = { m_pLightConstants->GetConstantBuffer(), m_pTransformConstants->GetConstantBuffer() };
         m_d3dContext->CSSetConstantBuffers(0 , 2, pBlurCsCbs);
         m_d3dContext->CSSetShaderResources(0, ARRAYSIZE(pSrv), pSrv);
         m_d3dContext->CSSetUnorderedAccessViews(0, ARRAYSIZE(pUav), pUav, initialCounts);
         m_d3dContext->Dispatch(CEIL_DIV(m_ShadowMapWidth,THREAD_GROUP_WIDTH), CEIL_DIV(m_ShadowMapHeight, THREAD_GROUP_HEIGHT), 1);

         m_d3dContext->CSSetShader(m_lightGeneratorCS, NULL, 0);
         m_d3dContext->Dispatch(CEIL_DIV((UINT)NUM_X_LIGHTS, THREAD_GROUP_WIDTH), CEIL_DIV((UINT)NUM_Y_LIGHTS, THREAD_GROUP_HEIGHT), 1);

         m_d3dContext->CSSetShaderResources(0, ARRAYSIZE(pNullSrv), pNullSrv);
         m_d3dContext->CSSetUnorderedAccessViews(0, 2, pNullUav, NULL);

         ID3D11ShaderResourceView *pLightCullingSrv[] = { 
            m_pLightBuffer->GetShaderResourceView(),
            m_pPrePassDepthStencil->GetShaderResourceView()
         };

         ID3D11UnorderedAccessView *pLightCullingUav[] = { 
            m_pLightsPerTileBuffer->GetUnorderedAccessView(),
            m_pTileLightIndexBuffer->GetUnorderedAccessView()
         };
         m_d3dContext->CSSetShader(m_lightCullingCS, NULL, 0);
         m_d3dContext->CSSetShaderResources(0, ARRAYSIZE(pLightCullingSrv), pLightCullingSrv);
         m_d3dContext->CSSetUnorderedAccessViews(0, ARRAYSIZE(pLightCullingUav), pLightCullingUav, NULL);

         m_d3dContext->Dispatch(CEIL_DIV(m_tileXCount, THREAD_GROUP_WIDTH), CEIL_DIV(m_tileYCount, THREAD_GROUP_HEIGHT), 1);
         m_d3dContext->CSSetShaderResources(0, 2, pNullSrv);
         m_d3dContext->CSSetUnorderedAccessViews(0, 2, pNullUav, NULL);

         // Prepare the setup for actual rendering
         ID3D11RenderTargetView *pFirstPassRtv[] = { 
            m_pPostProcessRTV->GetRenderTargetView(),
         };
         
         ID3D11ShaderResourceView *pShadowSrv[] = {
            m_pBlurredShadowSurface->GetShaderResourceView(),
            m_pLightBuffer->GetShaderResourceView(),
            m_pTileLightIndexBuffer->GetShaderResourceView(),
            m_pLightsPerTileBuffer->GetShaderResourceView()
         };
         
         m_d3dContext->RSSetViewports(1, &m_viewport);
         m_d3dContext->IASetInputLayout( m_inputLayout );
         m_d3dContext->VSSetShader(m_solidColorVS, 0, 0);
         m_d3dContext->OMSetRenderTargets(ARRAYSIZE(pFirstPassRtv), pFirstPassRtv, m_pPrePassDepthStencil->GetDepthStencilView());
         m_d3dContext->PSSetShaderResources(2 , ARRAYSIZE(pShadowSrv), pShadowSrv);

      }

      if( draw == DepthPrePass)
      {
         m_d3dContext->PSSetShader(m_prePassPS, 0, 0);
      }

      for (UINT i = 0; i < scene.size(); i++)
      {
         auto pMat = &m_matList[scene[i].m_MaterialIndex];
       
         if( draw != DepthPrePass)
         {
            if( pMat->m_texture )
            {
               if( draw == ShadowPass)
               {
                  m_d3dContext->PSSetShader(m_textureNoShadingPS, 0, 0);
               }
               else
               {
                  if(pMat->m_bump)
                  {
                     m_d3dContext->PSSetShader(m_bumpTexturePS, 0, 0);
                  }
                  else
                  {
                     m_d3dContext->PSSetShader(m_texturePS, 0, 0);
                  }
               }
            }
            else
            {            
               if( draw == MainRender)
               {
                  m_d3dContext->PSSetShader(m_diffuseNoShadingPS, 0, 0);
               }
               else
               {
                  m_d3dContext->PSSetShader(m_solidColorPS, 0, 0);
               }
            }
         }

         ID3D11ShaderResourceView *pMaterialSrv[] = {
            pMat->m_texture,
            pMat->m_bump
         };
         m_d3dContext->PSSetShaderResources(0 , ARRAYSIZE(pMaterialSrv), pMaterialSrv);

         if(i < scene.size())
         {
            m_d3dContext->IASetVertexBuffers( 0, 1, &scene[i].m_vertexBuffer, &stride,  &offset);
            m_d3dContext->IASetIndexBuffer(scene[i].m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            m_d3dContext->PSSetConstantBuffers(0 , 1, &pMat->m_materialConstantBuffer);
            m_d3dContext->DrawIndexed(scene[i].m_numIndices, 0, 0);
         }
      }
   }

   if(!g_shadowView)
   {

      ID3D11RenderTargetView *pNullRTVs[] = { 
          NULL, NULL, NULL, NULL
      };

#if REFLECTIONS_ON
      ID3D11RenderTargetView *pReflectionRTVs[] = { 
           m_pReflectionRTV->GetRenderTargetView()
      };

      ID3D11ShaderResourceView *pReflectionSRVs[] = {
         m_pPostProcessRTV->GetShaderResourceView(),
         m_pWorldPosMap->GetShaderResourceView(),
         m_pWorldRefMap->GetShaderResourceView(),
         m_pScreenReflMap->GetShaderResourceView(),
         m_pLinearDepthMap->GetShaderResourceView()
         //m_pPrePassDepthStencil->GetShaderResourceView(),
      };

      m_d3dContext->OMSetRenderTargets(ARRAYSIZE(pNullRTVs), pNullRTVs, NULL);
      m_d3dContext->PSSetShaderResources(0, ARRAYSIZE(pReflectionSRVs), pReflectionSRVs);
      m_d3dContext->OMSetRenderTargets(ARRAYSIZE(pReflectionRTVs), pReflectionRTVs, NULL);
      m_d3dContext->PSSetShader(m_reflectionPS, NULL, 0);
      m_d3dContext->VSSetShader(m_planeVS, NULL, 0);

      m_d3dContext->Draw(3, 0);
#endif

      ID3D11RenderTargetView *pBloomRTVs[] = { 
          m_pBrightMap->GetRenderTargetView()
      };

      ID3D11ShaderResourceView *pBloomSRVs[] = {
#if REFLECTIONS_ON
         m_pReflectionRTV->GetShaderResourceView(),
#else
		  m_pPostProcessRTV->GetShaderResourceView(),
#endif
         NULL,
         NULL,
         NULL
      };

      m_d3dContext->OMSetRenderTargets(ARRAYSIZE(pNullRTVs), pNullRTVs, NULL);
      m_d3dContext->PSSetShaderResources(0, ARRAYSIZE(pBloomSRVs), pBloomSRVs);
      m_d3dContext->OMSetRenderTargets(ARRAYSIZE(pBloomRTVs), pBloomRTVs, NULL);
      m_d3dContext->RSSetViewports(1, &m_bloomViewport);
      m_d3dContext->PSSetShader(m_bloomPS, NULL, 0);
      m_d3dContext->Draw(3, 0);

      ID3D11ShaderResourceView *pBlendSRVs[] = {
#if REFLECTIONS_ON
		  m_pReflectionRTV->GetShaderResourceView(),
#else
		  m_pPostProcessRTV->GetShaderResourceView(),
#endif
         m_pBrightMap->GetShaderResourceView()
      };

      ID3D11RenderTargetView *pFinalPassRTVs[] = { 
         m_backBufferTarget,
      };
      m_d3dContext->RSSetViewports(1, &m_viewport);
      m_d3dContext->OMSetRenderTargets(1, pFinalPassRTVs, NULL);
      m_d3dContext->PSSetShaderResources(0, ARRAYSIZE(pBlendSRVs), pBlendSRVs);
      m_d3dContext->VSSetShader(m_planeVS, NULL, 0);
      m_d3dContext->PSSetShader(m_blendPS, NULL, 0);
      m_d3dContext->Draw(3, 0);
   }

   // Clear our the SRVs
   ID3D11ShaderResourceView *pNullSrv[] = { NULL, NULL, NULL, NULL, NULL};
   m_d3dContext->PSSetShaderResources(1 , ARRAYSIZE(pNullSrv), pNullSrv);

#if 0
   parallel_for(blocked_range<size_t>(0, 3),
      Derp());
#endif 

   std::wostringstream woss;
   woss << std::fixed << std::showpoint << std::setprecision(2) << m_fps;       
   std::wstring ws = woss.str();

   m_pFontWrapper->DrawString(
		m_d3dContext,
		ws.c_str(),// String
		50.0f,// Font size
		25.0,// X position
		25.0f,// Y position
		0xff0099ff,// Text color, 0xAaBbGgRr
		FW1_RESTORESTATE);// Flags

   m_swapChain->Present(0, 0);
}

bool Renderer::LoadContent() 
{
   LARGE_INTEGER li;
   QueryPerformanceFrequency(&li);

   g_PCFreq = double(li.QuadPart)/1000.0;

   HRESULT hResult = FW1CreateFactory(FW1_VERSION, &m_pFW1Factory);
   if (FAILED(hResult))
	{
		DXTRACE_MSG("Failed to create font factory");
		return false;
	}
   hResult = m_pFW1Factory->CreateFontWrapper(m_d3dDevice, L"Arial", &m_pFontWrapper);
   if (FAILED(hResult))
	{
		DXTRACE_MSG("Failed to create font wrapper");
		return false;
	}

	m_viewport.Width = static_cast<FLOAT>(m_width);
	m_viewport.Height = static_cast<FLOAT>(m_height);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;

   m_bloomViewport.Height = static_cast<FLOAT>(m_height) / BLOOM_DIVISION;
   m_bloomViewport.Width = static_cast<FLOAT>(m_width) / BLOOM_DIVISION;
   m_bloomViewport.MinDepth = 0.0f;
   m_bloomViewport.MaxDepth = 1.0f;
   m_bloomViewport.TopLeftX = 0.0f;
	m_bloomViewport.TopLeftY = 0.0f;

   m_ShadowMapHeight = (UINT)SHADOW_HEIGHT;
   m_ShadowMapWidth = (UINT)SHADOW_WIDTH;
   m_pRWDepthBuffer = new RWDepthBuffer(m_d3dDevice, m_ShadowMapWidth, m_ShadowMapHeight);
   m_pBlurredShadowMap = new RWRenderTarget(m_d3dDevice, m_ShadowMapWidth, m_ShadowMapHeight);
   m_pShadowMapWorldPos = new RWRenderTarget(m_d3dDevice, m_ShadowMapWidth, m_ShadowMapHeight);
   m_pShadowMapWorldNorm = new RWRenderTarget(m_d3dDevice, m_ShadowMapWidth, m_ShadowMapHeight);

   m_pBlurredShadowSurface = new RWComputeSurface(m_d3dDevice,  m_ShadowMapWidth, m_ShadowMapHeight);
   m_pLightMap = new RWRenderTarget(m_d3dDevice, m_ShadowMapWidth, m_ShadowMapHeight);

   m_pPrePassDepthStencil = new RWDepthBuffer(m_d3dDevice, m_width, m_height);
   
   m_pWorldPosMap = new RWRenderTarget(m_d3dDevice, m_width, m_height);
   m_pWorldRefMap = new RWRenderTarget(m_d3dDevice, m_width, m_height);
   m_pScreenReflMap = new RWRenderTarget(m_d3dDevice, m_width, m_height);
   m_pReflectionRTV = new RWRenderTarget(m_d3dDevice, m_width, m_height);

   m_pLinearDepthMap = new RWRenderTarget(m_d3dDevice, m_width, m_height);

   m_pPostProcessRTV = new RWRenderTarget(m_d3dDevice, m_width, m_height);
   m_pBrightMap = new RWRenderTarget(
      m_d3dDevice, 
      (UINT)(static_cast<float>(m_width) / BLOOM_DIVISION), 
      (UINT)(static_cast<float>(m_height) / BLOOM_DIVISION));
   
   m_tileXCount = (UINT)NUM_X_TILES;
   m_tileYCount = (UINT)NUM_Y_TILES;
   UINT numTiles = (UINT)(NUM_TILES);
   UINT numLights = (UINT)(NUM_LIGHTS);

   m_pLightBuffer = new RWStructuredBuffer<PS_Point_Light>(m_d3dDevice, numLights, true);
   m_pTileLightIndexBuffer = new RWStructuredBuffer<UINT>(m_d3dDevice, (UINT)(numTiles * MAX_LIGHTS_PER_TILE));
   m_pLightsPerTileBuffer = new RWStructuredBuffer<UINT>(m_d3dDevice, numLights * numTiles);

   m_pPlaneRenderer = new PlaneRenderer(m_d3dDevice);

   D3D11_RASTERIZER_DESC rasterizerDesc;
   rasterizerDesc.FillMode = D3D11_FILL_SOLID;
   rasterizerDesc.CullMode = D3D11_CULL_BACK;
   rasterizerDesc.FrontCounterClockwise = FALSE;
   rasterizerDesc.DepthBias = 0;
   rasterizerDesc.SlopeScaledDepthBias = 0.0f;
   rasterizerDesc.DepthBiasClamp = 0.0;
   rasterizerDesc.DepthClipEnable = TRUE;
   rasterizerDesc.ScissorEnable = FALSE;
   rasterizerDesc.MultisampleEnable = FALSE;
   rasterizerDesc.AntialiasedLineEnable = FALSE;

   D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
   depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
   depthStencilDesc.DepthEnable = TRUE;
   depthStencilDesc.StencilEnable = FALSE;
   depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

   HRESULT rasterResult = m_d3dDevice->CreateRasterizerState(&rasterizerDesc, &m_rasterState);
	if (FAILED(rasterResult))
	{
		DXTRACE_MSG("Failed to create the rasterizer state");
		return false;
	}

   HRESULT depthResult = m_d3dDevice->CreateDepthStencilState(&depthStencilDesc, &m_depthState);
	if (FAILED(depthResult))
	{
		DXTRACE_MSG("Failed to create the depth state");
		return false;
	}

   // Create an instance of the Importer class
  Assimp::Importer importer;
  // And have it read the given file with some example postprocessing
  // Usually - if speed is not the most important aspect for you - you'll 
  // propably to request more postprocessing than we do in this example.
  const aiScene* AssimpScene = importer.ReadFile( "sponza.obj", 
        aiProcess_Triangulate            |
        aiProcess_MakeLeftHanded         | 
        aiProcess_FlipUVs                | 
        aiProcess_FlipWindingOrder       | 
        aiProcess_GenSmoothNormals       |
        aiProcess_CalcTangentSpace       |
        aiProcess_PreTransformVertices);

  InitializeMatMap( AssimpScene );
  for (UINT i = 0; i < AssimpScene->mNumMeshes; i++)
  {
     Mesh d3dMesh;
     const aiMesh *pMesh = AssimpScene->mMeshes[i];
     bool result = CreateD3DMesh(pMesh, AssimpScene, &d3dMesh);
     if ( result != true ) return false;
     scene.push_back(d3dMesh);
  }

  if (AssimpScene->HasCameras())
  {
     assert(AssimpScene->mNumCameras == 1);
     auto pCam = AssimpScene->mCameras[0];
     auto pos = pCam->mPosition;
     auto lookAt = pCam->mLookAt;
     auto up = pCam->mUp;
     
     XMFLOAT3 fPos(pos.x, pos.y, pos.z);
     XMFLOAT3 fLookAt(lookAt.x, lookAt.y, lookAt.z);
     XMFLOAT3 fUp(up.x, up.y, up.z);

     m_pCamera = new Camera(XMLoadFloat3(&fPos), XMLoadFloat3(&fLookAt), XMLoadFloat3(&fUp));
     m_nearPlane = pCam->mClipPlaneNear;
     m_farPlane = pCam->mClipPlaneFar;
     m_cameraUnit = 50.0f;
     m_fieldOfView = pCam->mHorizontalFOV * 2.0f / pCam->mAspect;
  }
  else
  {
     XMFLOAT3 pos(300.0f, 200.0f, 0.0f);
     XMFLOAT3 lookAt(1.0f, 0.0f, 0.0f);
     XMFLOAT3 up(0.0f, 1.0f, 0.0f);
     m_pCamera = new Camera(XMLoadFloat3(&pos), XMLoadFloat3(&lookAt), XMLoadFloat3(&up));

     m_nearPlane = 0.1;
     m_farPlane = 4000.0f;
     m_cameraUnit = 50.0f;
     m_fieldOfView = 3.14f / 2.0f;
  }

  m_pLightConstants = new ConstantBuffer<PS_Light_Constant_Buffer>(m_d3dDevice);

  if (AssimpScene->HasLights())
  {
#if 0
     assert(AssimpScene->mNumLights == 1);
     auto pLight = AssimpScene->mLights[0];
     
     assert(pLight->mType == aiLightSource_POINT);
     auto lightPos = pLight->mPosition;
     m_lightPosition = XMFLOAT4(lightPos.x, lightPos.y, lightPos.z, 1.0f);
#endif
  }
  else
  {
      m_lightDirection = LIGHT_DIRECTION;
      m_lightUp = LIGHT_UP;
  }

  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
  flags |= D3DCOMPILE_DEBUG;
#endif
   // Prefer higher CS shader profile when possible as CS 5.0 provides better performance on 11-class hardware.
  LPCSTR profile = ( m_d3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ) ? "cs_5_0" : "cs_4_0";
   const D3D_SHADER_MACRO defines[] = 
   {
       "EXAMPLE_DEFINE", "1",
       NULL, NULL
   };
   ID3DBlob* csBuffer = nullptr;
   HRESULT hr = D3DUtils::CompileD3DShader( "BlurCS.hlsl", "main", "cs_5_0", &csBuffer);

   HR(m_d3dDevice->CreateComputeShader( csBuffer->GetBufferPointer(), 
		                               csBuffer->GetBufferSize(), 
		                               NULL, &m_blurCS));

   hr = D3DUtils::CompileD3DShader( "LightCullingCS.hlsl", "main", "cs_5_0", &csBuffer);

   HR(m_d3dDevice->CreateComputeShader( csBuffer->GetBufferPointer(), 
		                               csBuffer->GetBufferSize(), 
		                               NULL, &m_lightCullingCS));

   hr = D3DUtils::CompileD3DShader( "LightGeneratorCS.hlsl", "main", "cs_5_0", &csBuffer);

   HR(m_d3dDevice->CreateComputeShader( csBuffer->GetBufferPointer(), 
		                               csBuffer->GetBufferSize(), 
		                               NULL, &m_lightGeneratorCS));

   ID3DBlob* vsBuffer = 0;
   BOOL compileResult = D3DUtils::CompileD3DShader("PlainVert.hlsl", "main", "vs_5_0", &vsBuffer);
   if( compileResult == false )
   {
      MessageBox(0, "Error loading vertex shader!", "Compile Error", MB_OK);
      return false;
   }
   HR(m_d3dDevice->CreateVertexShader(
         vsBuffer->GetBufferPointer(), 
         vsBuffer->GetBufferSize(), 
         0, 
         &m_solidColorVS));
   
   ID3DBlob* vsPlaneBuffer = 0;
   compileResult = D3DUtils::CompileD3DShader("PlaneVertexShader.hlsl", "main", "vs_5_0", &vsPlaneBuffer);
   if( compileResult == false )
   {
      MessageBox(0, "Error loading vertex shader!", "Compile Error", MB_OK);
      return false;
   }

   HR(m_d3dDevice->CreateVertexShader(
         vsPlaneBuffer->GetBufferPointer(), 
         vsPlaneBuffer->GetBufferSize(), 
         0,
         &m_planeVS));

   vsPlaneBuffer->Release();

   D3D11_INPUT_ELEMENT_DESC solidColorLayout[] =
   {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL",   1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL",   2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0}
   };

   unsigned int totalLayoutElements = ARRAYSIZE( solidColorLayout );

   HR(m_d3dDevice->CreateInputLayout( solidColorLayout, totalLayoutElements,
      vsBuffer->GetBufferPointer(), vsBuffer->GetBufferSize(), &m_inputLayout ));

   vsBuffer->Release();

   HR(D3DUtils::CreatePixelShader(
      m_d3dDevice,
      "reflectionPS.hlsl", 
      "main", 
      "ps_5_0", 
      &m_reflectionPS));

   HR(D3DUtils::CreatePixelShader(
      m_d3dDevice,
      "PrepassPS.hlsl", 
      "main", 
      "ps_5_0", 
      &m_prePassPS));

   HR(D3DUtils::CreatePixelShader(
      m_d3dDevice,
      "PlainPixel.hlsl", 
      "main", 
      "ps_5_0", 
      &m_solidColorPS));
   
   HR(D3DUtils::CreatePixelShader(
      m_d3dDevice,
      "BlendPS.hlsl", 
      "main", 
      "ps_5_0", 
      &m_blendPS));
   
   HR(D3DUtils::CreatePixelShader(
      m_d3dDevice,
      "PlainPixel.hlsl", 
      "texMain", 
      "ps_5_0", 
      &m_texturePS ));

   HR(D3DUtils::CreatePixelShader(
      m_d3dDevice,
      "PlainPixel.hlsl", 
      "bumpMain", 
      "ps_5_0", 
      &m_bumpTexturePS ));

   HR(D3DUtils::CreatePixelShader(
      m_d3dDevice,
      "TextureShader.hlsl", 
      "main", 
      "ps_5_0", 
      &m_textureNoShadingPS ));

   HR(D3DUtils::CreatePixelShader(
      m_d3dDevice,
      "TextureShader.hlsl", 
      "difMain", 
      "ps_5_0", 
      &m_diffuseNoShadingPS ));

   HR(D3DUtils::CreatePixelShader(
      m_d3dDevice,
      "BloomPS.hlsl", 
      "main", 
      "ps_5_0", 
      &m_bloomPS ));

   VS_Transformation_Constant_Buffer vsConstBuf;
   vsConstBuf.mvp = XMMatrixIdentity();

   m_pTransformConstants = new ConstantBuffer<VS_Transformation_Constant_Buffer>(m_d3dDevice);

   D3D11_SAMPLER_DESC colorMapDesc;
   ZeroMemory( &colorMapDesc, sizeof( colorMapDesc ));
   colorMapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
   colorMapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
   colorMapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
   colorMapDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
   colorMapDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
   colorMapDesc.MaxLOD = D3D11_FLOAT32_MAX;

   HR(m_d3dDevice->CreateSamplerState( &colorMapDesc, &m_colorMapSampler));

   D3D11_SAMPLER_DESC shadowSamplerDesc;
   ZeroMemory( &shadowSamplerDesc, sizeof( shadowSamplerDesc ));
   shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
   shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
   shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
   shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
   shadowSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
   shadowSamplerDesc.BorderColor[0] = 1.0f;
   shadowSamplerDesc.BorderColor[1] = 1.0f;
   shadowSamplerDesc.BorderColor[2] = 1.0f;
   shadowSamplerDesc.BorderColor[3] = 1.0f;
   shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

   HR(m_d3dDevice->CreateSamplerState( &shadowSamplerDesc, &m_shadowSampler));

   D3D11_SAMPLER_DESC blackBorderSamplerDesc;
   ZeroMemory( &blackBorderSamplerDesc, sizeof( blackBorderSamplerDesc ));
   blackBorderSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
   blackBorderSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
   blackBorderSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
   blackBorderSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
   blackBorderSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
   blackBorderSamplerDesc.BorderColor[0] = 0.0f;
   blackBorderSamplerDesc.BorderColor[1] = 0.0f;
   blackBorderSamplerDesc.BorderColor[2] = 0.0f;
   blackBorderSamplerDesc.BorderColor[3] = 0.0f;
   blackBorderSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

   HR(m_d3dDevice->CreateSamplerState( &blackBorderSamplerDesc, &m_blackBorderSampler));

   return true;
}

void Renderer::UnloadContent() 
{
   delete m_pRWDepthBuffer;
   delete m_pLightMap;
   delete m_pPostProcessRTV;
   delete m_pBrightMap;
   delete m_pReflectionRTV;


   delete m_pWorldPosMap;
   delete m_pWorldRefMap;
   delete m_pScreenReflMap;

   delete m_pLinearDepthMap;

   delete m_pLightBuffer;
   delete m_pTileLightIndexBuffer;
   delete m_pLightsPerTileBuffer;
   
   delete m_pBlurredShadowMap;
   delete m_pShadowMapWorldPos;
   delete m_pShadowMapWorldNorm;

   delete m_pBlurredShadowSurface;
   delete m_pPrePassDepthStencil;

   delete m_pTransformConstants;
   delete m_pLightConstants;
   delete m_pPlaneRenderer;
   delete m_pCamera;

   DestroyMatMap();
   for(UINT i = 0; i < scene.size(); i++)
   {
      DestroyD3DMesh(&scene[i]);
   }

   if( m_solidColorPS ) m_solidColorPS->Release();
   if( m_solidColorVS ) m_solidColorVS->Release();
   if( m_inputLayout ) m_inputLayout->Release();
   if( m_colorMapSampler ) m_colorMapSampler->Release();

   m_pFontWrapper->Release();
	m_pFW1Factory->Release();
}