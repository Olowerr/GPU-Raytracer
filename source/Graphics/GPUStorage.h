#pragma once

#include "DirectX/DX11.h"

class GPUStorage
{
public:
	GPUStorage();
	GPUStorage(uint32_t elementByteWidth, uint32_t capacity, void* pData);
	~GPUStorage();

	void shutdown();
	void initiate(uint32_t elementByteWidth, uint32_t capacity, void* pData);

	template<typename UpdateFunction>
	void update(uint32_t newCapacity, UpdateFunction function);
	void updateRaw(uint32_t newCapacity, void* pData);

	inline ID3D11ShaderResourceView* getSRV() const;

private:
	ID3D11Buffer* m_pBuffer;
	ID3D11ShaderResourceView* m_pSRV;

	uint32_t m_capacity;
	uint32_t m_elementByteWidth;
};

inline ID3D11ShaderResourceView* GPUStorage::getSRV() const { return m_pSRV; }

template<typename UpdateFunction>
inline void GPUStorage::update(uint32_t newCapacity, UpdateFunction function)
{
	if (m_capacity != newCapacity && newCapacity)
		initiate(m_elementByteWidth, newCapacity, nullptr);

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	D3D11_MAPPED_SUBRESOURCE sub{};
	if (FAILED(pDevCon->Map(m_pBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &sub)))
		return;

	function((char*)sub.pData);

	pDevCon->Unmap(m_pBuffer, 0u);
}
