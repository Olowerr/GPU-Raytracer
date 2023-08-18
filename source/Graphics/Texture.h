#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"

class Texture
{
public:
	Texture(unsigned char* pTextureData, uint32_t width, uint32_t height)
		:m_pTextureData(pTextureData), m_width(width), m_height(height)
	{
		OKAY_ASSERT(pTextureData);
	}

	~Texture() = default;

	inline uint32_t getWidth() const;
	inline uint32_t getHeight() const;
	inline unsigned char* getTextureData() const;


private:
	uint32_t m_width, m_height;
	unsigned char* m_pTextureData;
};

inline uint32_t Texture::getWidth()	const 				{return m_width;		}
inline uint32_t Texture::getHeight() const				{return m_height;		}
inline unsigned char* Texture::getTextureData() const	{return m_pTextureData;	}