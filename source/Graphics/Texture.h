#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"

/*
	Textures are not primarily responsible for allocating/deallocating its data.
	They'll only deallocate its old data during move.
*/

class Texture
{
public:
	Texture(unsigned char* pTextureData, uint32_t width, uint32_t height, std::string_view name)
		:m_pTextureData(pTextureData), m_width(width), m_height(height), m_name(name.data())
	{
		OKAY_ASSERT(pTextureData);
	}
	~Texture() = default;

	Texture(const Texture& other) = default;

	Texture& operator=(Texture&& other) noexcept
	{
		OKAY_DELETE_ARRAY(m_pTextureData);

		m_pTextureData = other.m_pTextureData;
		other.m_pTextureData = nullptr;

		m_width = other.m_width;
		m_height = other.m_height;

		other.m_width = other.m_height = 0u;

		m_name = other.m_name;
		other.m_name = "";

		return *this;
	}

	inline const std::string& getName() const;
	inline uint32_t getWidth() const;
	inline uint32_t getHeight() const;
	inline unsigned char* getTextureData() const;


private:
	std::string m_name;
	uint32_t m_width, m_height;
	unsigned char* m_pTextureData;
};

inline const std::string& Texture::getName() const { return m_name; }
inline uint32_t Texture::getWidth() const { return m_width; }
inline uint32_t Texture::getHeight() const { return m_height; }
inline unsigned char* Texture::getTextureData() const { return m_pTextureData; }