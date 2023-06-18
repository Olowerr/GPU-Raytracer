#pragma once
#include <Utilities.h>

#include <d3d11.h>

namespace Okay
{
	void initiateDX11();
	void shutdownDX11();

	ID3D11Device* getDevice();
	ID3D11DeviceContext* getDeviceContext();

	bool createSwapChain(IDXGISwapChain** ppSwapChain, HWND hWnd);
}