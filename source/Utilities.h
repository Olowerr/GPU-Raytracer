#pragma once
#include <assert.h>
#include <stdint.h>
#include <memory>
#include <fstream>

#ifdef DIST
#define OKAY_ASSERT(condition) 
#else
#define OKAY_ASSERT(condition) assert(condition)
#endif

#define DX11_RELEASE(x) if (x) {x->Release(); x = nullptr; }0
#define OKAY_DELETE(X)		 if (X) { delete X;		X = nullptr; }
#define OKAY_DELETE_ARRAY(X) if (X) { delete[]X;	X = nullptr; }

namespace Okay
{
	constexpr uint32_t INVALID_UINT = ~0u;

	static bool readBinary(std::string_view binPath, std::string& output)
	{
		std::ifstream reader(binPath.data(), std::ios::binary);
		if (!reader)
			return false;

		reader.seekg(0, std::ios::end);
		output.reserve((size_t)reader.tellg());
		reader.seekg(0, std::ios::beg);

		output.assign(std::istreambuf_iterator<char>(reader), std::istreambuf_iterator<char>());

		return true;
	}

	static std::string_view getFileEnding(std::string_view path)
	{
		return path.substr(path.find_last_of('.'));
	}

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	static inline Ref<T> createRef(Args&&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}
