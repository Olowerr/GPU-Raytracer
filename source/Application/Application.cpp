#include "Application.h"
#include "DirectX/DX11Utilities.h"

Application::Application()
{
	glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

	bool glInit = glfwInit();
	OKAY_ASSERT(glInit);
}

Application::~Application()
{
	glfwTerminate();
}

void Application::run()
{
	// Window Test
	ID3D11Device* pDevice = nullptr;
	ID3D11DeviceContext* pDeviceContext = nullptr;
	OkayDX11::createDevice(&pDevice, &pDeviceContext);

	m_window.initiate(1600, 900, "GPU Raytracer", pDevice);

	ID3D11Texture2D* backBuffer = nullptr;
	ID3D11UnorderedAccessView* pUAV = nullptr;

	m_window.m_pDX11SwapChain->GetBuffer(0u, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	OKAY_ASSERT(backBuffer);

	pDevice->CreateUnorderedAccessView(backBuffer, nullptr, &pUAV);
	OKAY_ASSERT(pUAV);
	
	float clearColour[4] = { 1.f, 0.7f, 0.5f, 0.f };
	while (m_window.isOpen())
	{
		m_window.processMessages();
	
		pDeviceContext->ClearUnorderedAccessViewFloat(pUAV, clearColour);

		m_window.present();
	}

	DX11_RELEASE(pDevice);
	DX11_RELEASE(pDeviceContext);
	DX11_RELEASE(backBuffer);
	DX11_RELEASE(pUAV);

}
