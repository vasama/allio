#pragma once

#include <allio/section_handle.hpp>

namespace allio {

enum class page_access : uint8_t
{
	read                                = 1 << 0,
	write                               = 1 << 1,
	execute                             = 1 << 2,

	read_write = read | write,
};
vsm_flag_enum(page_access);


enum class page_size : uint8_t
{
	automatic                           = 0,
	smallest                            = 1,
	largest                             = 2,
	exact                               = 3,

	_4KB                                = 1 << 2,
	_64KB                               = 1 << 3,
	_2MB                                = 1 << 4,
	_512MB                              = 1 << 5,
	_1GB                                = 1 << 6,

	any = automatic | _4KB | _64KB | _2MB | _512MB | _1GB,

	mode_mask = automatic | smallest | largest | exact,
	size_mask = _4KB | _64KB | _2MB | _512MB | _1GB,

#if vsm_arch_x86
#	define vsm_detail_page_size_x86(x) x
#else
#	define vsm_detail_page_size_x86(x) 0
#endif

#if vsm_arch_arm
#	define vsm_detail_page_size_arm(x) x
#else
#	define vsm_detail_page_size_arm(x) 0
#endif

	x86_4KB                             = vsm_detail_page_size_x86(_4KB),
	x86_2MB                             = vsm_detail_page_size_x86(_2MB),
	x86_1GB                             = vsm_detail_page_size_x86(_1GB),

	arm_4KB                             = vsm_detail_page_size_arm(_4KB),
	arm_64KB                            = vsm_detail_page_size_arm(_64KB),
	arm_2MB                             = vsm_detail_page_size_arm(_2MB),
	arm_512MB                           = vsm_detail_page_size_arm(_512MB),
	arm_1GB                             = vsm_detail_page_size_arm(_1GB),

#undef vsm_detail_page_size_x86
#undef vsm_detail_page_size_arm
};
vsm_flag_enum(page_size);

page_size get_supported_page_sizes();


namespace detail {

class map_handle_base : handle
{
	using final_handle_type = final_handle<map_handle_base>;

	vsm::linear<void*> m_base;
	vsm::linear<size_t> m_size;

public:
	using base_type = handle;


	#define allio_map_handle_map_parameters(type, data, ...) \
		type(allio::map_handle, map_parameters) \
		data(::uintptr_t, preferred_address, nullptr) \
		data(::allio::page_access, access, ::allio::page_access::read_write) \
		data(::allio::page_size, page_size, ::allio::page_size::any) \

	allio_interface_parameters(allio_map_handle_map_parameters)


	[[nodiscard]] void* base() const
	{
		return m_base.value;
	}

	[[nodiscard]] size_t size() const
	{
		return m_size.value;
	}


	template<parameters<map_parameters> P = map_parameters::interface>
	vsm::result<void> map(section_handle const& section, file_size const offset, size_t const size, P const& args = {})
	{
		return block_map(section, offset, size, args);
	}

private:
	vsm::result<void> block_map(section_handle const& section, file_size const offset, size_t const size, map_parameters const& args);

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::map> const& args);
};

vsm::result<final_handle<map_handle_base>> block_map(
	section_handle const& section,
	file_offset const offset,
	size_t const size,
	map_handle_base::map_parameters const& args);

vsm::result<final_handle<map_handle_base>> block_map_anonymous(
	size_t const size,
	map_handle_base::map_parameters const& args);

} // namespace detail

using map_handle = final_handle<detail::map_handle_base>;

template<parameters<map_handle::map_parameters> P = map_handle::map_parameters::interface>
vsm::result<map_handle> map(section_handle const& section, file_offset const offset, size_t const size, P const& args = {})
{
	return detail::block_map(section, offset, size, args);
}

template<parameters<map_handle::map_parameters> P = map_handle::map_parameters::interface>
vsm::result<map_handle> map_anonymous(size_t const size, P const& args = {})
{
	return detail::block_map_anonymous(size, args);
}

allio_detail_api extern allio_handle_implementation(map_handle);

} // namespace allio
