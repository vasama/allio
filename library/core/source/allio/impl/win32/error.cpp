#include <allio/impl/error_encoding_impl.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/win32/kernel_error.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

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


static constexpr uint32_t full_high_bits = 2;
static constexpr uint32_t full_code_bits = 32 - full_high_bits;
static constexpr uint32_t part_code_bits = ec::code_bits - full_high_bits;

static constexpr uint32_t full_code_mask = (static_cast<uint32_t>(1) << full_code_bits) - 1;
static constexpr uint32_t part_code_mask = (static_cast<uint32_t>(1) << part_code_bits) - 1;

template<>
uint32_t ec::encode_error_code(kernel_error const e)
{
	uint32_t const bits = static_cast<uint32_t>(e);
	uint32_t const code = bits & full_code_mask;

	if (code <= part_code_mask)
	{
		uint32_t const high = bits >> full_code_bits;
		return high << part_code_bits | code;
	}

	return 0;
}

template<>
kernel_error ec::decode_error_code(uint32_t const e)
{
	uint32_t const code = e & part_code_mask;
	uint32_t const high = e >> part_code_bits;

	return static_cast<kernel_error>(high << full_code_bits | code);
}

template class ec::encoded_error_category<allio_error_encoding, kernel_error>;
