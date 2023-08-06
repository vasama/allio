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


enum class page_level : uint8_t
{
	default_level                       = 0,

	_4KiB                               = 12,
	_16KiB                              = 14,
	_64KiB                              = 16,
	_512KiB                             = 19,
	_1MiB                               = 20,
	_2MiB                               = 21,
	_8MiB                               = 23,
	_16MiB                              = 24,
	_32MiB                              = 25,
	_256MiB                             = 28,
	_512MiB                             = 29,
	_1GiB                               = 30,
	_2GiB                               = 31,
	_16GiB                              = 34,
};

inline page_level get_page_level(size_t const size)
{
	vsm_assert(size > 1 && vsm::math::is_power_of_two(size));
	return static_cast<page_level>(std::countr_zero(size));
}

inline size_t get_page_size(page_level const level)
{
	vsm_assert(level != page_level::default_level);
	return static_cast<size_t>(1) << static_cast<uint8_t>(level);
}

std::span<page_level const> get_supported_page_levels();


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
		data(::allio::page_level, page_level, ::allio::page_level::default_level) \

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
