#pragma once

#include <vsm/allocator.hpp>
#include <vsm/result.hpp>

#include <cstddef>

namespace allio::detail {

class buffer_registrar
{
public:
	[[nodiscard]] virtual vsm::result<vsm::allocation> allocate_buffers(size_t min_size);
	void deallocate_buffers(vsm::allocation storage);

	[[nodiscard]] virtual vsm::result<void*> register_buffers(
		std::byte* storage,
		size_t buffer_size,
		size_t buffer_count) = 0;

	virtual void deregister_buffers(void* opaque_pointer) = 0;


	[[nodiscard]] friend bool operator==(buffer_registrar const& lhs, buffer_registrar const& rhs)
	{
		return &lhs == &rhs;
	}

protected:
	buffer_registrar() = default;
	buffer_registrar(buffer_registrar const&) = default;
	buffer_registrar& operator=(buffer_registrar const&) = default;
	~buffer_registrar() = default;
};

} // namespace allio::detail
