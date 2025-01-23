#include <allio/impl/error_encoding_impl.hpp>
#include <allio/impl/linux/error.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

template<>
uint32_t ec::encode_error_code(system_error const e)
{
	if (static_cast<uint32_t>(e) <= ec::code_mask)
	{
		return static_cast<uint32_t>(e);
	}

	return 0;
}

template<>
system_error ec::decode_error_code(uint32_t const e)
{
	return static_cast<system_error>(e);
}

template class ec::encoded_error_category<allio_error_encoding, system_error>;
