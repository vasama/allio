#pragma once

#include <allio/section_handle.hpp>

namespace allio {

namespace io {

struct map;

} // namespace io

namespace detail {

class map_handle_base : public handle
{
	using final_handle_type = final_handle<map_handle_base>;

	vsm::linear<void*> m_base;
	vsm::linear<size_t> m_size;

public:
	struct implementation;

	using base_type = handle;

	struct native_handle_type : base_type::native_handle_type
	{
		void* base;
		size_t size;
	};


	#define allio_map_handle_map_parameters(type, data, ...) \
		type(allio::map_handle, map_parameters) \
		data(::allio::section_handle const*,    section,        nullptr) \
		data(::allio::file_size,                offset,         0) \
		data(::uintptr_t,                       address,        0) \
		data(::allio::page_access,              access,         ::allio::page_access::read_write) \
		data(::allio::page_level,               page_level,     ::allio::page_level::default_level) \

	allio_interface_parameters(allio_map_handle_map_parameters);


	constexpr map_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			m_base.value,
			m_size.value,
		};
	}


	[[nodiscard]] page_level get_page_level() const
	{
		vsm_assert(*this);
		//TODO: Implement map_handle::get_page_level.
		return {};
	}

	[[nodiscard]] size_t get_page_size() const
	{
		return allio::get_page_size(get_page_level());
	}


	[[nodiscard]] void* base() const
	{
		vsm_assert(*this);
		return m_base.value;
	}

	[[nodiscard]] size_t size() const
	{
		vsm_assert(*this);
		return m_size.value;
	}


	vsm::result<void> set_page_access(void* base, size_t size, page_access access);


	template<parameters<map_parameters> P = map_parameters::interface>
	vsm::result<void> map(section_handle const& section, file_size const offset, size_t const size, P const& args = {})
	{
		return block_map(section, offset, size, args);
	}

private:
	bool check_address_range(void* base, size_t size) const;

	vsm::result<void> block_map(section_handle const& section, file_size const offset, size_t const size, map_parameters const& args);
	vsm::result<void> block_close();

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::map> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::close> const& args);
};

vsm::result<final_handle<map_handle_base>> block_map(
	section_handle const& section,
	file_size const offset,
	size_t const size,
	map_handle_base::map_parameters const& args);

vsm::result<final_handle<map_handle_base>> block_map_anonymous(
	size_t const size,
	map_handle_base::map_parameters const& args);

} // namespace detail

using map_handle = final_handle<detail::map_handle_base>;

template<parameters<map_handle::map_parameters> P = map_handle::map_parameters::interface>
vsm::result<map_handle> map(section_handle const& section, file_size const offset, size_t const size, P const& args = {})
{
	return detail::block_map(section, offset, size, args);
}

template<parameters<map_handle::map_parameters> P = map_handle::map_parameters::interface>
vsm::result<map_handle> map_anonymous(size_t const size, P const& args = {})
{
	return detail::block_map_anonymous(size, args);
}

allio_detail_api extern allio_handle_implementation(map_handle);


template<typename T>
class map_span
{
	static_assert(std::is_trivially_copyable_v<T>);

	map_handle m_handle;

public:
	explicit map_span(map_handle handle)
		: m_handle(vsm_move(handle))
	{
	}

	[[nodiscard]] T* data() const
	{
		return reinterpret_cast<T*>(m_handle.base());
	}

	[[nodiscard]] size_t size() const
	{
		return m_handle.size() / sizeof(T);
	}

	[[nodiscard]] T& operator[](size_t const offset) const
	{
		return data()[offset];
	}

	[[nodiscard]] T* begin() const
	{
		return data();
	}

	[[nodiscard]] T* end() const
	{
		return data() + size();
	}
};


template<>
struct io::parameters<io::map>
	: map_handle::map_parameters
{
	using handle_type = map_handle;
	using result_type = void;

	size_t size;
};

} // namespace allio
