#include <cassert>
#include "Camera.h"

Camera::Camera(XMVECTOR position, XMVECTOR lookAt, XMVECTOR up) :
   m_pos(position), m_CameraDirty(TRUE) 
{
   m_up = XMVector3Normalize(up);
   m_lookAt = XMVector3Normalize(lookAt);


   m_left = XMVector3Normalize(XMVector3Cross(lookAt, up));
}

Camera::~Camera() {}

const XMMATRIX *Camera::GetViewMatrix() const
{
   if( m_CameraDirty )
   {
      m_viewMat = XMMatrixLookAtLH(m_pos, m_pos + m_lookAt, m_up);
   }

   m_CameraDirty = FALSE;
   return &m_viewMat;
}

XMVECTOR Camera::GetLeft() const
{
   XMVECTOR left = XMVector3Normalize(m_left);
   return XMVectorSet(XMVectorGetX(left), XMVectorGetY(left), XMVectorGetZ(left), 0.0f);
}

XMVECTOR Camera::GetEye() const
{
   XMVECTOR eye = XMVector3Normalize(m_lookAt);
   return XMVectorSet(XMVectorGetX(eye), XMVectorGetY(eye), XMVectorGetZ(eye), 0.0f);
}

XMVECTOR Camera::GetPos() const
{
   return XMVectorSet(XMVectorGetX(m_pos), XMVectorGetY(m_pos), XMVectorGetZ(m_pos), 0.0f);
}
   
void Camera::MoveCamera(XMVECTOR delta)
{
   m_pos = XMVectorAdd(XMVectorScale(m_lookAt, XMVectorGetZ(delta)), m_pos);
   m_pos = XMVectorAdd(XMVectorScale(m_left, XMVectorGetX(delta)), m_pos);
   m_pos = XMVectorAdd(XMVectorScale(m_up, XMVectorGetY(delta)), m_pos);
   m_CameraDirty = true;
}

void Camera::RotateCameraHorizontally(float radians)
{
   XMVECTOR yAxis = XMVectorSet(0, 1, 0, 0);
   XMMATRIX rotateMat = XMMatrixRotationAxis(m_up, radians);

   // TODO: Is all this work necessary?
   m_lookAt = XMVector3Transform(m_lookAt, rotateMat);
   m_left = XMVector3Transform(m_left, rotateMat);
   m_CameraDirty = true;
}

void Camera::RotateCameraVertically(float radians)
{
   XMMATRIX rotateMat = XMMatrixRotationAxis(m_left, radians);

   // TODO: Is all this work necessary?
   m_lookAt = XMVector3Transform(m_lookAt, rotateMat);
   m_up = XMVector3Transform(m_up, rotateMat);
   m_CameraDirty = true;
}