#pragma once
#include "DirectX/DX11.h"
#include "glm/glm.hpp"

class RenderTexture
{
public:
	enum BitPos : uint32_t
	{
		B_RENDER		 = 0,
		B_SHADER_READ	 = 1,
		B_SHADER_WRITE	 = 2,
		B_DEPTH			 = 3,
	};

	enum Format : uint32_t
	{
		INVALID,
		F_8X1,
		F_8X4,
		F_32X4,
	};

	enum Flags : uint32_t
	{
		RENDER			= 1 << BitPos::B_RENDER,
		SHADER_READ		= 1 << BitPos::B_SHADER_READ,
		SHADER_WRITE	= 1 << BitPos::B_SHADER_WRITE,
		DEPTH			= 1 << BitPos::B_DEPTH,
	};

	RenderTexture();
	RenderTexture(ID3D11Texture2D* texture, uint32_t flags);
	RenderTexture(uint32_t width, uint32_t height, uint32_t flags, Format format = Format::F_8X4);
	~RenderTexture();
	void shutdown();

	void create(ID3D11Texture2D* texture, uint32_t flags);
	void create(uint32_t width, uint32_t height, uint32_t flags, Format format = Format::F_8X4);

	void clear();
	void clear(float* colour);
	void clear(const glm::vec4& colour);

	void resize(uint32_t width, uint32_t height);
	glm::ivec2 getDimensions() const;

	inline uint32_t getFlags() const;
	inline bool valid() const;

	inline ID3D11Texture2D* getTexture() const;
	inline ID3D11RenderTargetView* getRTV() const;
	inline ID3D11ShaderResourceView* getSRV() const;
	inline ID3D11UnorderedAccessView* getUAV() const;

	inline ID3D11Texture2D* getDepthBuffer();
	inline ID3D11DepthStencilView* getDSV() const;

private:
	uint32_t flags;
	Format format;

	ID3D11Texture2D* buffer;
	ID3D11RenderTargetView* rtv;
	ID3D11ShaderResourceView* srv;
	ID3D11UnorderedAccessView* uav;

	ID3D11Texture2D* depthBuffer;
	ID3D11DepthStencilView* dsv;

	void readFlgs(uint32_t flags);
};

inline uint32_t RenderTexture::getFlags() const { return flags; }
inline bool RenderTexture::valid() const		{ return buffer; }

inline ID3D11Texture2D* RenderTexture::getTexture() const			{ return buffer; }
inline ID3D11RenderTargetView* RenderTexture::getRTV() const		{ return rtv; }
inline ID3D11ShaderResourceView* RenderTexture::getSRV() const		{ return srv; }
inline ID3D11UnorderedAccessView* RenderTexture::getUAV() const		{ return uav; }

inline ID3D11Texture2D* RenderTexture::getDepthBuffer()				{ return depthBuffer; }
inline ID3D11DepthStencilView* RenderTexture::getDSV() const		{ return dsv; }
