#pragma once

#include <allio/detail/handles/file.hpp>
#include <allio/memory.hpp>

#include <vsm/standard.hpp>

namespace allio::detail {

struct protection_parameter_t
{
	detail::protection protection;

	struct tag_t
	{
		struct argument_t
		{
			detail::protection value;
			
			friend void tag_invoke(set_argument_t, protection_parameter_t& args, argument_t const& value)
			{
				args.protection = value.value;
			}
		};

		argument_t vsm_static_operator_invoke(detail::protection const protection)
		{
			return { protection };
		}
	};
};
inline constexpr protection_parameter_t::tag_t section_protection;

struct section_t : platform_object_t
{
	using base_type = platform_object_t;

	struct native_type : base_type::native_type
	{
		detail::protection protection;
	};

	struct create_t
	{
		using mutation_tag = producer_t;

		struct required_params_type
		{
			file_handle const* backing_file;
			file_size maximum_size;
		};

		using optional_params_type = protection_parameter_t;
	};

	using operations = type_list_cat<
		base_type::operations,
		type_list<create_t>
	>;

	template<typename H>
	struct abstract_interface : base_type::abstract_interface<H>
	{
		[[nodiscard]] detail::protection protection() const
		{
			return static_cast<H const&>(*this).native().native_type::protection;
		}
	};
};

namespace _section_blocking {

using section_handle = blocking_handle<section_t>;

vsm::result<section_handle> create_section(
	file_size const maximum_size,
	auto&&... args)
{
	vsm::result<section_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<section_t::create_t>(
		*r,
		io_args<section_t::create_t>(nullptr, maximum_size)(vsm_forward(args)...)));
	return r;
}

vsm::result<section_handle> create_section(
	_file_blocking::file_handle const& backing_file,
	file_size const maximum_size,
	auto&&... args)
{
	vsm::result<section_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<section_t::create_t>(
		*r,
		io_args<section_t::create_t>(&backing_file, maximum_size)(vsm_forward(args)...)));
	return r;
}

} // namespace _section_blocking

} // namespace allio::detail
