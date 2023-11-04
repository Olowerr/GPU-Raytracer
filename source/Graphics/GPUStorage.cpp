#include "GPUStorage.h"
#include "Utilities.h"

GPUStorage::GPUStorage()
	:m_pBuffer(nullptr), m_pSRV(nullptr), m_capacity(0u), m_elementByteWidth(0u)
{
}

GPUStorage::GPUStorage(uint32_t elementByteWidth, uint32_t capacity, void* pData)
{
	initiate(elementByteWidth, capacity, pData);
}

GPUStorage::~GPUStorage()
{
	shutdown();
}

void GPUStorage::shutdown()
{
	DX11_RELEASE(m_pBuffer);
	DX11_RELEASE(m_pSRV);

	m_capacity = 0u;
	m_elementByteWidth = 0u;
}

void GPUStorage::initiate(uint32_t elementByteWidth, uint32_t capacity, void* pData)
{
	OKAY_ASSERT(elementByteWidth);
	OKAY_ASSERT(capacity);

	shutdown();

	m_elementByteWidth = elementByteWidth;
	m_capacity = capacity;

	bool success = Okay::createStructuredBuffer(&m_pBuffer, &m_pSRV, pData, elementByteWidth, capacity);
	OKAY_ASSERT(success);
}

void GPUStorage::update(uint32_t newCapacity, void* pData)
{
	OKAY_ASSERT(pData);
	OKAY_ASSERT(newCapacity);

	if (m_capacity != newCapacity)
		initiate(m_elementByteWidth, newCapacity, pData);

	Okay::updateBuffer(m_pBuffer, pData, (size_t)m_elementByteWidth * newCapacity);
}