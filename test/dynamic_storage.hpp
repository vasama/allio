#pragma once

#include <allio/storage.hpp>

namespace allio {

class dynamic_storage
{
	std::unique_ptr<std::byte> m_storage;
	size_t m_size;

public:
	dynamic_storage(allio::storage_requirements const& requirements)
		: m_storage(static_cast<std::byte*>(operator new(requirements.size, std::align_val_t(requirements.alignment))))
		, m_size(requirements.size)
	{
	}

	allio::storage_ptr get()
	{
		return allio::storage_ptr(m_storage.get(), m_size);
	}
};

} // namespace allio
