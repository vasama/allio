#include <allio/map_handle.hpp>

#include <vsm/utility.hpp>

using namespace allio;
using namespace allio::detail;

bool map_handle_base::check_native_handle(native_handle_type const& handle)
{
	return base_type::check_native_handle(handle);
}

void map_handle_base::set_native_handle(native_handle_type const& handle)
{
	base_type::set_native_handle(handle);

	if (handle.section.flags[flags::not_null])
	{
		vsm_verify(m_section.set_native_handle(handle.section));
	}

	m_base.value = handle.base;
	m_size.value = handle.size;
	m_page_level.value = handle.page_level;
}

map_handle_base::native_handle_type map_handle_base::release_native_handle()
{
	return
	{
		base_type::release_native_handle(),
		m_section.release_native_handle(private_t()),
		m_base.release(),
		m_size.release(),
		m_page_level.release(),
	};
}

bool map_handle_base::check_address_range(void const* const range_base, size_t const range_size) const
{
	void const* const valid_beg = m_base.value;
	void const* const range_beg = range_base;

	void const* const valid_end = static_cast<char const*>(valid_beg) + m_size.value;
	void const* const range_end = static_cast<char const*>(range_beg) + range_size;

	return
		std::less_equal<>()(valid_beg, range_beg) &&
		std::less_equal<>()(range_end, valid_end);
}


vsm::result<void> map_handle_base::block_map(size_t const size, map_parameters const& args)
{
	return block<io::map>(static_cast<map_handle&>(*this), args, size);
}


#if 0
vsm::result<map_handle> block_map_section(section_handle const& section, file_size const offset, size_t const size, map_handle::basic_map_parameters const& args)
{
	vsm::result<map_handle> r(vsm::result_value);
	vsm_try_void(r->map(size, map_handle::map_parameters
	{
		&section,
		offset,
		args,
	}));
	return r;
}

vsm::result<map_handle> block_map_anonymous(size_t const size, map_handle::basic_map_parameters const& args)
{
	vsm::result<map_handle> r(vsm::result_value);
	vsm_try_void(r->map(size, map_handle::map_parameters
	{
		nullptr,
		0,
		args,
	}));
	return r;
}
#endif
