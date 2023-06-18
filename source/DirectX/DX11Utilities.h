#pragma once
#include <Utilities.h>

#include <d3d11.h>

namespace OkayDX11
{
	static bool createDevice(ID3D11Device** ppDevice, ID3D11DeviceContext** ppDeviceContext)
	{
		OKAY_ASSERT(ppDevice);
		OKAY_ASSERT(ppDeviceContext);

		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
		uint32_t flags = 0;
#ifndef DIST	
		flags = D3D11_CREATE_DEVICE_DEBUG;
#endif

		return SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, 
			&featureLevel, 1u, D3D11_SDK_VERSION, ppDevice, nullptr, ppDeviceContext));
	}

	static bool createSwapChain(ID3D11Device* pDevice, IDXGISwapChain** ppSwapChain, HWND hWnd)
	{
		OKAY_ASSERT(pDevice);
		OKAY_ASSERT(ppSwapChain);
		OKAY_ASSERT(hWnd);

		DXGI_SWAP_CHAIN_DESC desc{};
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		desc.BufferCount = 1u;
		desc.BufferUsage = DXGI_USAGE_UNORDERED_ACCESS;

		desc.BufferDesc.Width = 0u; // 0u defaults to the window dimensions
		desc.BufferDesc.Height = 0u;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		desc.BufferDesc.RefreshRate.Numerator = 0u;
		desc.BufferDesc.RefreshRate.Denominator = 1u;

		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		desc.OutputWindow = hWnd;
		desc.Windowed = true;
		desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		IDXGIDevice* idxDevice = nullptr;
		IDXGIAdapter* adapter = nullptr;
		IDXGIFactory* factory = nullptr;

		pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&idxDevice);
		idxDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&adapter);
		adapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory);

		factory->CreateSwapChain(pDevice, &desc, ppSwapChain);
		DX11_RELEASE(idxDevice);
		DX11_RELEASE(adapter);
		DX11_RELEASE(factory);

		return *ppSwapChain;
	}
}