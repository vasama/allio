#pragma once

#include <allio/file_handle.hpp>
#include <allio/memory.hpp>

namespace allio {
namespace detail {

class section_handle_impl : public platform_handle
{
protected:
	using base_type = platform_handle;

private:
	vsm::linear<protection> m_protection;

public:
	struct native_handle_type : base_type::native_handle_type
	{
		protection protection;
	};

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			m_protection.value,
		};
	}


	[[nodiscard]] protection get_protection() const
	{
		vsm_assert(*this); //PRECONDITION
		return m_protection.value;
	}


	struct create_tag;

	#define allio_section_handle_create_parameters(type, data, ...) \
		type(allio::section_handle, create_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(::allio::protection, protection, ::allio::protection::read_write) \

	allio_interface_parameters(allio_section_handle_create_parameters);

protected:
	allio_detail_default_lifetime(section_handle_impl);

	section_handle_impl(unchecked_tag, native_handle_type const& native)
		: base_type(native)
		, m_protection(native.protection)
	{
	}

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<create_parameters> P = create_parameters::interface>
		vsm::result<void> create(P const& args) &;
	};
};

} // namespace detail

using section_handle = basic_handle<detail::section_handle_impl>;


template<parameters<section_handle::create_parameters> P = section_handle::create_parameters::interface>
vsm::result<section_handle> create_section(file_size const maximum_size, P const& args = {});


template<>
struct io_operation_traits<section_handle::create_tag>
{
	using handle_type = section_handle;
	using result_type = void;
	using params_type = section_handle::create_parameters;

	file_handle const* backing_file;
	file_size maximum_size;
};

} // namespace allio
