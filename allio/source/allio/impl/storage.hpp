#pragma once

namespace allio {

template<size_t Size, size_t Alignment>
class storage_thing
{
	alignas(Alignment) unsigned char storage[Size];

	template<typename T>
	friend T& new_object(storage_thing& storage)
	{
		static_assert(sizeof(T) <= Size);
		static_assert(alignof(T) <= Alignment);
		return *new T;
	}

	template<typename T>
	friend T& get_object(storage_thing& storage)
	{
		return *std::launder(reinterpret_cast<T*>(storage.storage));
	}

	template<typename T>
	friend T const& get_object(storage_thing const& storage)
	{
		return *std::launder(reinterpret_cast<T const*>(storage.storage));
	}
};

} // namespace allio
