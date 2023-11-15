#pragma once

#include <allio/detail/handles/section.hpp>

namespace allio {
namespace detail {

class _map_handle : public handle
{
protected:
	using base_type = handle;

	section_handle m_section;
	vsm::linear<void*> m_base;
	vsm::linear<size_t> m_size;
	vsm::linear<page_level> m_page_level;

public:
	struct native_handle_type : base_type::native_handle_type
	{
		section_handle::native_handle_type section;

		void* base;
		size_t size;

		allio::page_level page_level;
	};

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			section.get_native_handle(),
			m_base.value,
			m_size.value,
			m_page_level.value,
		};
	}


	[[nodiscard]] section_handle const& get_section() const
	{
		vsm_assert(*this); //PRECONDITION
		return m_section;
	}

	[[nodiscard]] void* base() const
	{
		vsm_assert(*this); //PRECONDITION
		return m_base.value;
	}

	[[nodiscard]] size_t size() const
	{
		vsm_assert(*this); //PRECONDITION
		return m_size.value;
	}


	[[nodiscard]] page_level get_page_level() const
	{
		vsm_assert(*this); //PRECONDITION
		return m_page_level.value;
	}

	[[nodiscard]] size_t get_page_size() const
	{
		return allio::get_page_size(get_page_level());
	}


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


	using decommit_parameters = no_parameters;


	using set_protection_parameters = no_parameters;

protected:
	allio_detail_default_lifetime(_map_handle);

	explicit _map_handle(native_handle_type const& native)
		: base_type(native)
		, m_section(private_t(), native.section)
		, m_base(native.base)
		, m_size(native.size)
		, m_page_level(native.page_level)
	{
	}

	[[nodiscard]] static bool check_native_handle(native_handle_type const& native)
	{
		return base_type::check_native_handle(native)
			&& section_handle::check_native_handle(native.section)
			&& native.base != nullptr
			&& native.size != 0
		;
	}

	void set_native_handle(native_handle_type const& native)
	{
		base_type::set_native_handle(native);
		m_section.set_native_handle(native);
		m_base.value = native.base;
		m_size.value = native.size;
		m_page_level = native.page_level;
	}

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<map_parameters> P = map_parameters::interface>
		vsm::result<void> map(size_t const size, P const& args = {}) &;

		template<parameters<commit_parameters> P = commit_parameters::interface>
		vsm::result<void> commit(void* const base, size_t const size, P const& args = {}) const;

		template<parameters<decommit_parameters> P = decommit_parameters::interface>
		vsm::result<void> decommit(void* const base, size_t const size, P const& args = {}) const;

		template<parameters<set_protection_parameters> P = set_protection_parameters::interface>
		vsm::result<void> set_protection(void* const base, size_t const size, protection const protection, P const& args = {}) const;
	};
};

} // namespace detail
} // namespace allio
