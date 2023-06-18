#pragma once

#include "DirectX/DX11.h"
#include "Utilities.h"

class Renderer
{
public:
	Renderer();
	Renderer(ID3D11Texture2D* pTarget);
	~Renderer();

	void shutdown();
	void initiate(ID3D11Texture2D* pTarget);

	void render();

private:
	ID3D11Texture2D* m_pBuffer;
	ID3D11UnorderedAccessView* m_pBufferUAV;
	uint32_t m_width, m_height;

	ID3D11ComputeShader* m_pMainRaytracingCS;
};