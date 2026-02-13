#pragma once
#include "../dxsdk/include/d3d8.h"

class RenderTarget
{
public:
    RenderTarget(LPDIRECT3DDEVICE8 device, int width, int height);
    ~RenderTarget();

    void BeginScene();
    void EndScene();
    void Blit(int destX = 0, int destY = 0);

    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    LPDIRECT3DDEVICE8 m_device = nullptr;
    LPDIRECT3DTEXTURE8 m_texture = nullptr;
    LPDIRECT3DSURFACE8 m_surface = nullptr;
    LPDIRECT3DSURFACE8 m_oldSurface = nullptr;
    LPDIRECT3DSURFACE8 m_depthStencilSurface = nullptr;
    LPDIRECT3DSURFACE8 m_oldDepthStencilSurface = nullptr;
    D3DVIEWPORT8 m_viewport = {};

    int m_width = 0;
    int m_height = 0;
};
