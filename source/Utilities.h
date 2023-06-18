#pragma once
#include <assert.h>
#include <stdint.h>
#include <memory>

#ifdef DIST
#define OKAY_ASSERT(condition) assert(condition)
#else
#define OKAY_ASSERT(condition) 
#endif

#define DX11_RELEASE(x) if (x) {x->Release(); x = nullptr; }0

namespace Okay
{
	constexpr uint32_t INVALID_UINT = ~0u;

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	static inline Ref<T> createRef(Args&&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}
