#include <cmath>
#include "RenderManager.h"

namespace Render
{
	namespace Fonts
	{
		LPD3DXFONT Default;
		LPD3DXFONT Menu;
		LPD3DXFONT MenuBold;
		LPD3DXFONT MenuText;
		LPD3DXFONT MenuTabs;
		LPD3DXFONT Text;
		LPD3DXFONT Clock;
		LPD3DXFONT Tabs;
	};
};

namespace
{
    LPD3DXFONT CreateFontHandle(LPDIRECT3DDEVICE8 device, int height, int width, int weight, BOOL italic, const char* face)
    {
        LOGFONT logFont = {};
        logFont.lfHeight = height;
        logFont.lfWidth = width;
        logFont.lfWeight = weight;
        logFont.lfItalic = italic;
        logFont.lfCharSet = DEFAULT_CHARSET;
        logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
        logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        logFont.lfQuality = ANTIALIASED_QUALITY;
        logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

#ifdef UNICODE
        wchar_t wideFace[LF_FACESIZE] = {};
        MultiByteToWideChar(CP_ACP, 0, face, -1, wideFace, LF_FACESIZE);
        wcsncpy_s(logFont.lfFaceName, wideFace, _TRUNCATE);
#else
        strncpy_s(logFont.lfFaceName, face, _TRUNCATE);
#endif

        LPD3DXFONT font = nullptr;
        if (FAILED(D3DXCreateFontIndirect(device, &logFont, &font)))
            return nullptr;

        return font;
    }
}

void Render::Initialise(LPDIRECT3DDEVICE8 pDevice)
{
    Fonts::Default = CreateFontHandle(pDevice, 14, 0, FW_NORMAL, FALSE, "Arial");
    Fonts::Menu = CreateFontHandle(pDevice, 24, 0, FW_NORMAL, FALSE, "DINPro-Regular");
    Fonts::MenuBold = CreateFontHandle(pDevice, 15, 0, FW_BOLD, FALSE, "Courier New");
    Fonts::MenuText = CreateFontHandle(pDevice, 18, 0, FW_NORMAL, FALSE, "Calibri");
    Fonts::MenuTabs = CreateFontHandle(pDevice, 18, 0, FW_BOLD, FALSE, "Arial");
    Fonts::Clock = CreateFontHandle(pDevice, 22, 0, FW_BOLD, FALSE, "Arial");
    Fonts::Tabs = CreateFontHandle(pDevice, 28, 0, FW_BOLD, FALSE, "Arial");
}

void Render::Draw(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR color)
{
	const D3DRECT rect = { x, y, x + w, y + h };
	pDevice->Clear(1, &rect, D3DCLEAR_TARGET, color, 0.0f, 0);
}

void Render::Outline(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR color)
{
	CUSTOMVERTEX vertices[5] = 
	{
		{ (float)x, (float)y, 0.0f, 1.0f, color },
		{ (float)(x + w), (float)y, 0.0f, 1.0f, color },
		{ (float)(x + w), (float)(y + h), 0.0f, 1.0f, color },
		{ (float)x, (float)(y + h), 0.0f, 1.0f, color },
		{ (float)x, (float)y, 0.0f, 1.0f, color }
	};

	pDevice->SetVertexShader(D3DFVF_CUSTOMVERTEX);
	pDevice->DrawPrimitiveUP(D3DPT_LINESTRIP, 4, vertices, sizeof(CUSTOMVERTEX));
}

void Render::Line(LPDIRECT3DDEVICE8 pDevice, int x, int y, int x2, int y2, D3DCOLOR color)
{
	CUSTOMVERTEX vertices[2] = 
	{
		{ (float)x, (float)y, 0.0f, 1.0f, color },
		{ (float)x2, (float)y2, 0.0f, 1.0f, color }
	};

	pDevice->SetVertexShader(D3DFVF_CUSTOMVERTEX);
	pDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, vertices, sizeof(CUSTOMVERTEX));
}

void Render::DrawOutlinedRect(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR col)
{
	Outline(pDevice, x, y, w, h, col);
}

void Render::DrawLine(LPDIRECT3DDEVICE8 pDevice, int x0, int y0, int x1, int y1, D3DCOLOR col)
{
	Line(pDevice, x0, y0, x1, y1, col);
}

void Render::Polygon(LPDIRECT3DDEVICE8 pDevice, int count, CUSTOMVERTEX* Vertexs, D3DCOLOR color)
{
	pDevice->SetVertexShader(D3DFVF_CUSTOMVERTEX);
	pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, count - 2, Vertexs, sizeof(CUSTOMVERTEX));
}

void Render::PolygonOutline(LPDIRECT3DDEVICE8 pDevice, int count, CUSTOMVERTEX* Vertexs, D3DCOLOR color, D3DCOLOR colorLine)
{
	Polygon(pDevice, count, Vertexs, color);
	
	for (int i = 0; i < count - 1; i++)
	{
		Line(pDevice, Vertexs[i].x, Vertexs[i].y, Vertexs[i + 1].x, Vertexs[i + 1].y, colorLine);
	}
	Line(pDevice, Vertexs[count - 1].x, Vertexs[count - 1].y, Vertexs[0].x, Vertexs[0].y, colorLine);
}

void Render::PolyLine(LPDIRECT3DDEVICE8 pDevice, int count, CUSTOMVERTEX* Vertexs, D3DCOLOR colorLine)
{
	for (int i = 0; i < count - 1; i++)
	{
		Line(pDevice, Vertexs[i].x, Vertexs[i].y, Vertexs[i + 1].x, Vertexs[i + 1].y, colorLine);
	}
}

void Render::DrawSquare(LPDIRECT3DDEVICE8 pDevice, int x, int y, int size, D3DCOLOR color)
{
	if (!pDevice || size <= 0)
		return;

	CUSTOMVERTEX vertices[4] =
	{
		{ static_cast<float>(x), static_cast<float>(y), 0.0f, 1.0f, color },
		{ static_cast<float>(x + size), static_cast<float>(y), 0.0f, 1.0f, color },
		{ static_cast<float>(x), static_cast<float>(y + size), 0.0f, 1.0f, color },
		{ static_cast<float>(x + size), static_cast<float>(y + size), 0.0f, 1.0f, color }
	};

	pDevice->SetVertexShader(D3DFVF_CUSTOMVERTEX);
	pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));
}

void Render::GradientV(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR c1, D3DCOLOR c2)
{
	CUSTOMVERTEX vertices[4] = 
	{
		{ (float)x, (float)y, 0.0f, 1.0f, c1 },
		{ (float)(x + w), (float)y, 0.0f, 1.0f, c1 },
		{ (float)x, (float)(y + h), 0.0f, 1.0f, c2 },
		{ (float)(x + w), (float)(y + h), 0.0f, 1.0f, c2 }
	};

	pDevice->SetVertexShader(D3DFVF_CUSTOMVERTEX);
	pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));
}

void Render::GradientH(LPDIRECT3DDEVICE8 pDevice, int x, int y, int w, int h, D3DCOLOR c1, D3DCOLOR c2)
{
	CUSTOMVERTEX vertices[4] = 
	{
		{ (float)x, (float)y, 0.0f, 1.0f, c1 },
		{ (float)(x + w), (float)y, 0.0f, 1.0f, c2 },
		{ (float)x, (float)(y + h), 0.0f, 1.0f, c1 },
		{ (float)(x + w), (float)(y + h), 0.0f, 1.0f, c2 }
	};

	pDevice->SetVertexShader(D3DFVF_CUSTOMVERTEX);
	pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));
}

void Render::DrawCircle(LPDIRECT3DDEVICE8 pDevice, float x, float y, float r, float segments, D3DCOLOR color)
{
	const int vertexCount = static_cast<int>(segments) + 1;
	CUSTOMVERTEX* vertices = new CUSTOMVERTEX[vertexCount];
	for (int i = 0; i < vertexCount; i++)
	{
		const float theta = 2.0f * D3DX_PI * static_cast<float>(i) / segments;
		vertices[i] = { x + r * std::cos(theta), y + r * std::sin(theta), 0.0f, 1.0f, color };
	}

	pDevice->SetVertexShader(D3DFVF_CUSTOMVERTEX);
	pDevice->DrawPrimitiveUP(D3DPT_LINESTRIP, segments, vertices, sizeof(CUSTOMVERTEX));

	delete[] vertices;
}

void Render::Text(LPD3DXFONT pFont, int x, int y, D3DCOLOR color, const char* text, DWORD format)
{
	RECT Rect = { x, y, x, y };
	pFont->DrawTextA(text, -1, &Rect, format | DT_NOCLIP, color);
}
void Render::TextW(LPD3DXFONT pFont, int x, int y, D3DCOLOR color, const wchar_t* text, DWORD format)
{
	RECT Rect = { x, y, x, y };
	pFont->DrawTextW(text, -1, &Rect, format | DT_NOCLIP, color);
}

void Render::TextOutlined(LPD3DXFONT pFont, int x, int y, D3DCOLOR color, const char* text,
	D3DCOLOR outlineColor, int thickness, DWORD format)
{
	if (!pFont || !text)
		return;

	const int radius = max(1, thickness);
	const POINT offsets[] = {
		{ -radius,  0 }, { radius, 0 }, { 0, -radius }, { 0, radius },
		{ -radius, -radius }, { radius, -radius }, { -radius, radius }, { radius, radius }
	};

	for (const POINT& offset : offsets)
	{
		RECT rect = { x + offset.x, y + offset.y, x + offset.x, y + offset.y };
		pFont->DrawTextA(text, -1, &rect, format | DT_NOCLIP, outlineColor);
	}

	Text(pFont, x, y, color, text, format);
}
void Render::TextWOutlined(LPD3DXFONT pFont, int x, int y, D3DCOLOR color, const wchar_t* text,
	D3DCOLOR outlineColor, int thickness, DWORD format)
{
	if (!pFont || !text)
		return;

	const int radius = max(1, thickness);
	const POINT offsets[] = {
		{ -radius,  0 }, { radius, 0 }, { 0, -radius }, { 0, radius },
		{ -radius, -radius }, { radius, -radius }, { -radius, radius }, { radius, radius }
	};

	for (const POINT& offset : offsets)
	{
		RECT rect = { x + offset.x, y + offset.y, x + offset.x, y + offset.y };
		pFont->DrawTextW(text, -1, &rect, format | DT_NOCLIP, outlineColor);
	}

	TextW(pFont, x, y, color, text, format);
}

RECT Render::GetTextSize(LPD3DXFONT pFont, const char* text)
{
	RECT rect;
	pFont->DrawTextA(text, -1, &rect, DT_CALCRECT, 0xFFFFFFFF);
	return rect;
}

bool Render::WorldToScreen(D3DXVECTOR3 &in, D3DXVECTOR3 &out, LPDIRECT3DDEVICE8 pDevice)
{
	D3DVIEWPORT8 viewport;
	pDevice->GetViewport(&viewport);

	D3DXMATRIX projection, view, world;
	pDevice->GetTransform(D3DTS_PROJECTION, &projection);
	pDevice->GetTransform(D3DTS_VIEW, &view);
	pDevice->GetTransform(D3DTS_WORLD, &world);

	D3DXVec3Project(&out, &in, &viewport, &projection, &view, &world);

	return (out.z < 1.0f);
}

RECT Render::GetViewport(LPDIRECT3DDEVICE8 pDevice)
{
	D3DVIEWPORT8 viewport;
	pDevice->GetViewport(&viewport);
	RECT rect = { viewport.X, viewport.Y, viewport.X + viewport.Width, viewport.Y + viewport.Height };
	return rect;
}

void Render::Shutdown()
{
	auto releaseFont = [](LPD3DXFONT& font)
	{
		if (font)
		{
			font->Release();
			font = nullptr;
		}
	};

	releaseFont(Fonts::Default);
	releaseFont(Fonts::Menu);
	releaseFont(Fonts::MenuBold);
	releaseFont(Fonts::MenuText);
	releaseFont(Fonts::MenuTabs);
	releaseFont(Fonts::Text);
	releaseFont(Fonts::Clock);
	releaseFont(Fonts::Tabs);
}
