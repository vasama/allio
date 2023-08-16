#pragma once

#include <allio/section_handle.hpp>

#include <optional>

namespace allio {

namespace io {

struct map;

} // namespace io

namespace detail {

class map_handle_base : public handle
{
	using final_handle_type = final_handle<map_handle_base>;

	section_handle m_section;
	vsm::linear<void*> m_base;
	vsm::linear<size_t> m_size;
	vsm::linear<page_level> m_page_level;

public:
	using base_type = handle;

	struct native_handle_type : base_type::native_handle_type
	{
		section_handle::native_handle_type section;

		void* base;
		size_t size;

		allio::page_level page_level;
	};
	
	using async_operations = type_list_cat<
		base_type::async_operations,
		type_list<
			io::map
		>
	>;


	//TODO: Use optional with sentinel for page_level.
	#define allio_map_handle_basic_map_parameters(type, data, ...) \
		type(allio::detail::map_handle_base, basic_map_parameters) \
		data(::uintptr_t,                               address,            0) \
		data(bool,                                      commit,             true) \
		data(::allio::protection,                       protection,         ::allio::protection::read_write) \
		data(::std::optional<::allio::page_level>,      page_level) \

	allio_interface_parameters(allio_map_handle_basic_map_parameters);

	#define allio_map_handle_map_parameters(type, data, ...) \
		type(allio::detail::map_handle_base, map_parameters) \
		data(::allio::section_handle const*,            section,            nullptr) \
		data(::allio::file_size,                        section_offset,     0) \
		allio_map_handle_basic_map_parameters(__VA_ARGS__, __VA_ARGS__) \

	allio_interface_parameters(allio_map_handle_map_parameters);

	#define allio_map_handle_commit_parameters(type, data, ...) \
		type(allio::detail::map_handle_base, commit_parameters) \
		data(::std::optional<::allio::protection>,      protection) \

	allio_interface_parameters(allio_map_handle_commit_parameters);


	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			m_section.get_native_handle(),
			m_base.value,
			m_size.value,
			m_page_level.value,
		};
	}


	[[nodiscard]] section_handle const& get_section() const
	{
		return m_section;
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


	[[nodiscard]] page_level get_page_level() const
	{
		vsm_assert(*this);
		return m_page_level.value;
	}

	[[nodiscard]] size_t get_page_size() const
	{
		return allio::get_page_size(get_page_level());
	}


	template<parameters<map_parameters> P = map_parameters::interface>
	vsm::result<void> map(size_t const size, P const& args = {})
	{
		return block_map(size, args);
	}

	template<parameters<map_parameters> P = map_parameters::interface>
	vsm::result<void> commit(void* const base, size_t const size, P const& args = {}) const
	{
		return block_commit(base, size, args);
	}

	vsm::result<void> decommit(void* base, size_t size) const;

	vsm::result<void> set_protection(void* base, size_t size, protection protection) const;

protected:
	using base_type::base_type;

	static bool check_native_handle(native_handle_type const& handle);
	void set_native_handle(native_handle_type const& handle);
	native_handle_type release_native_handle();

private:
	bool check_address_range(void const* base, size_t size) const;

	vsm::result<void> block_map(size_t size, map_parameters const& args);
	vsm::result<void> block_commit(void* base, size_t size, commit_parameters const& args) const;

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::map> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::close> const& args);
};

vsm::result<final_handle<map_handle_base>> block_map_section(
	section_handle const& section,
	file_size const offset,
	size_t const size,
	map_handle_base::basic_map_parameters const& args);

vsm::result<final_handle<map_handle_base>> block_map_anonymous(
	size_t const size,
	map_handle_base::basic_map_parameters const& args);

} // namespace detail

using map_handle = final_handle<detail::map_handle_base>;

template<parameters<map_handle::basic_map_parameters> P = map_handle::basic_map_parameters::interface>
vsm::result<map_handle> map_section(section_handle const& section, file_size const offset, size_t const size, P const& args = {})
{
	return detail::block_map_section(section, offset, size, args);
}

template<parameters<map_handle::basic_map_parameters> P = map_handle::basic_map_parameters::interface>
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
