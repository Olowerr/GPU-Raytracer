#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"

class Scene;

class Renderer
{
public:
	Renderer();
	Renderer(ID3D11Texture2D* pTarget, Scene* pScene);
	~Renderer();

	void shutdown();
	void initiate(ID3D11Texture2D* pTarget, Scene* pScene);

	inline void setScene(Scene* pScene);
	void render();

private: // Scene
	Scene* m_pScene;

private: // DX11
	ID3D11Texture2D* m_pTargetTexture;
	ID3D11UnorderedAccessView* m_pTargetUAV;
	uint32_t m_width, m_height;

	ID3D11ComputeShader* m_pMainRaytracingCS;

	ID3D11Buffer* m_pSphereDataBuffer;
	ID3D11ShaderResourceView* m_pSphereDataSRV;
};

inline void Renderer::setScene(Scene* pScene)
{
	OKAY_ASSERT(pScene);
	m_pScene = pScene;
}