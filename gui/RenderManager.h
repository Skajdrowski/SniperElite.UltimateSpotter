#pragma once
#include "../dxsdk/include/d3dx8.h"

struct CUSTOMVERTEX
{
    FLOAT x, y, z, rhw; 
    DWORD color;        
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

namespace Render
{
	void Initialise(LPDIRECT3DDEVICE8 pDevice);

	void Draw(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR color);
	void Outline(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR color);
	void Line(LPDIRECT3DDEVICE8 pDevice, int x, int y, int x2, int y2, D3DCOLOR color);
	void DrawOutlinedRect(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR col);
	void DrawLine(LPDIRECT3DDEVICE8 pDevice, int x0, int y0, int x1, int y1, D3DCOLOR col);
	void Polygon(LPDIRECT3DDEVICE8 pDevice, int count, CUSTOMVERTEX* Vertexs, D3DCOLOR color);
	void PolygonOutline(LPDIRECT3DDEVICE8 pDevice, int count, CUSTOMVERTEX* Vertexs, D3DCOLOR color, D3DCOLOR colorLine);
	void PolyLine(LPDIRECT3DDEVICE8 pDevice, int count, CUSTOMVERTEX* Vertexs, D3DCOLOR colorLine);
	void DrawSquare(LPDIRECT3DDEVICE8 pDevice, int x, int y, int size, D3DCOLOR color);

	void GradientV(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR c1, D3DCOLOR c2);
	void DrawCircle(LPDIRECT3DDEVICE8 pDevice, float x, float y, float r, float segments, D3DCOLOR color);
	void GradientH(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR c1, D3DCOLOR c2);

	namespace Fonts
	{
		extern LPD3DXFONT Default;
		extern LPD3DXFONT Menu;
		extern LPD3DXFONT MenuBold;
		extern LPD3DXFONT MenuText;
		extern LPD3DXFONT MenuTabs;
		extern LPD3DXFONT Text;
		extern LPD3DXFONT Clock;
		extern LPD3DXFONT Tabs;
	};

	void Text(LPD3DXFONT pFont, int x, int y, D3DCOLOR color, const char* text, DWORD format = 0);
	void TextW(LPD3DXFONT pFont, int x, int y, D3DCOLOR color, const wchar_t* text, DWORD format = 0);
	void TextOutlined(LPD3DXFONT pFont, int x, int y, D3DCOLOR color, const char* text,
		D3DCOLOR outlineColor, int thickness = 1, DWORD format = 0);
	void TextWOutlined(LPD3DXFONT pFont, int x, int y, D3DCOLOR color, const wchar_t* text,
		D3DCOLOR outlineColor, int thickness = 1, DWORD format = 0);
	RECT GetTextSize(LPD3DXFONT pFont, const char* text);

	bool WorldToScreen(D3DXVECTOR3 &in, D3DXVECTOR3 &out, LPDIRECT3DDEVICE8 pDevice);
	RECT GetViewport(LPDIRECT3DDEVICE8 pDevice);
	void Shutdown();
};
