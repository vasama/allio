#pragma once

#include <allio/detail/handles/file_handle.hpp>
#include <allio/memory.hpp>

namespace allio {
namespace detail {

class _section_handle : public platform_handle
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


	struct create_t;

	#define allio_section_handle_create_parameters(type, data, ...) \
		type(allio::section_handle, create_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(::allio::protection, protection, ::allio::protection::read_write) \

	allio_interface_parameters(allio_section_handle_create_parameters);

protected:
	allio_detail_default_lifetime(_section_handle);

	_section_handle(private_t, native_handle_type const& native)
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

template<>
struct io_operation_traits<section_handle::create_t>
{
	using handle_type = section_handle;
	using result_type = void;
	using params_type = section_handle::create_parameters;

	file_handle const* backing_file;
	file_size maximum_size;
};

} // namespace allio
