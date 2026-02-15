#include "RenderTarget.h"
#include "RenderManager.h"
#include <stdexcept>

RenderTarget::RenderTarget(LPDIRECT3DDEVICE8 device, int width, int height)
    : m_device(device), m_width(width), m_height(height), m_valid(false)
{
    if (!m_device)
        return;

    if (FAILED(m_device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_texture)) || !m_texture)
        return;

    if (FAILED(m_texture->GetSurfaceLevel(0, &m_surface)) || !m_surface)
        return;

    if (FAILED(m_device->CreateDepthStencilSurface(width, height, D3DFMT_D16,
        D3DMULTISAMPLE_NONE, &m_depthStencilSurface)))
        return;

    m_viewport.X = 0;
    m_viewport.Y = 0;
    m_viewport.Width = width;
    m_viewport.Height = height;
    m_viewport.MinZ = 0.0f;
    m_viewport.MaxZ = 1.0f;

    m_valid = true;
}

RenderTarget::~RenderTarget()
{
    if (m_depthStencilSurface)
        m_depthStencilSurface->Release();
    if (m_surface)
        m_surface->Release();
    if (m_texture)
        m_texture->Release();
}

void RenderTarget::BeginScene()
{
    m_device->GetRenderTarget(&m_oldSurface);
    m_device->GetDepthStencilSurface(&m_oldDepthStencilSurface);
    m_device->SetRenderTarget(m_surface, m_depthStencilSurface);
    m_device->SetViewport(&m_viewport);
    m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
}

void RenderTarget::EndScene()
{
    if (m_oldSurface)
    {
        m_device->SetRenderTarget(m_oldSurface, m_oldDepthStencilSurface);
        m_oldSurface->Release();
        m_oldSurface = nullptr;
    }
    if (m_oldDepthStencilSurface)
    {
        m_oldDepthStencilSurface->Release();
        m_oldDepthStencilSurface = nullptr;
    }
}

void RenderTarget::Blit(int destX, int destY)
{
    struct Vertex
    {
        float x, y, z, rhw;
        float u, v;
    };

    const float left = static_cast<float>(destX) - 0.5f;
    const float top = static_cast<float>(destY) - 0.5f;
    const float right = left + static_cast<float>(m_width);
    const float bottom = top + static_cast<float>(m_height);

    Vertex vertices[4] =
    {
        { left, top, 0.0f, 1.0f, 0.0f, 0.0f },
        { right, top, 0.0f, 1.0f, 1.0f, 0.0f },
        { left, bottom, 0.0f, 1.0f, 0.0f, 1.0f },
        { right, bottom, 0.0f, 1.0f, 1.0f, 1.0f },
    };

    DWORD oldMinFilter, oldMagFilter;
    DWORD oldColorOp, oldColorArg1, oldAlphaOp, oldAlphaArg1;
    DWORD oldColorOp1, oldAlphaOp1;
    m_device->GetTextureStageState(0, D3DTSS_MINFILTER, &oldMinFilter);
    m_device->GetTextureStageState(0, D3DTSS_MAGFILTER, &oldMagFilter);
    m_device->GetTextureStageState(0, D3DTSS_COLOROP, &oldColorOp);
    m_device->GetTextureStageState(0, D3DTSS_COLORARG1, &oldColorArg1);
    m_device->GetTextureStageState(0, D3DTSS_ALPHAOP, &oldAlphaOp);
    m_device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &oldAlphaArg1);
    m_device->GetTextureStageState(1, D3DTSS_COLOROP, &oldColorOp1);
    m_device->GetTextureStageState(1, D3DTSS_ALPHAOP, &oldAlphaOp1);

    m_device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
    m_device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    m_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    m_device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    m_device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
    m_device->SetTexture(0, m_texture);
    m_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));

    m_device->SetTextureStageState(0, D3DTSS_MINFILTER, oldMinFilter);
    m_device->SetTextureStageState(0, D3DTSS_MAGFILTER, oldMagFilter);
    m_device->SetTextureStageState(0, D3DTSS_COLOROP, oldColorOp);
    m_device->SetTextureStageState(0, D3DTSS_COLORARG1, oldColorArg1);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAOP, oldAlphaOp);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, oldAlphaArg1);
    m_device->SetTextureStageState(1, D3DTSS_COLOROP, oldColorOp1);
    m_device->SetTextureStageState(1, D3DTSS_ALPHAOP, oldAlphaOp1);
}
