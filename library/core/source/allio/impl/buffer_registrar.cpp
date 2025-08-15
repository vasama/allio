#include <allio/detail/buffer_registrar.hpp>

#include <allio/impl/error_encoding.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<vsm::allocation> buffer_registrar::allocate_buffers(size_t const min_size)
{
	return vsm::unexpected(allio_error(error::unsupported_operation));
}

void buffer_registrar::deallocate_buffers(vsm::allocation const storage)
{
	vsm_unreachable();
}
