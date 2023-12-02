#pragma once

#include <allio/detail/handles/file.hpp>
#include <allio/memory.hpp>

#include <vsm/standard.hpp>

namespace allio::detail {

struct protection_t
{
	detail::protection protection;

	friend void tag_invoke(set_argument_t, protection_t& args, detail::protection const protection)
	{
		args.protection = protection;
	}
};

namespace section_io {

struct create_t
{
	using operation_concept = producer_t;
	struct required_params_type
	{
		platform_object_t::native_type const* backing_file;
		fs_size max_size;
	};
	using optional_params_type = protection_t;
	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, create_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, create_t> const& a)
		requires requires { Object::create(h, a); }
	{
		return Object::create(h, a);
	}
};

} // namespace section_io

struct section_t : platform_object_t
{
	using base_type = platform_object_t;

	struct native_type : base_type::native_type
	{
		detail::protection protection;
	};

	using create_t = section_io::create_t;

	using operations = type_list_append<
		base_type::operations
		, create_t
	>;

	template<typename Handle>
	struct abstract_interface : base_type::abstract_interface<Handle>
	{
		[[nodiscard]] detail::protection protection() const
		{
			return static_cast<Handle const&>(*this).native().native_type::protection;
		}
	};
};
using abstract_section_handle = abstract_handle<section_t>;


namespace _section_b {

using section_handle = blocking_handle<section_t>;

vsm::result<section_handle> create_section(fs_size const max_size, auto&&... args)
{
	vsm::result<section_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<section_t, section_io::create_t>(
		*r,
		make_io_args<section_t, section_io::create_t>(nullptr, max_size)(vsm_forward(args)...)));
	return r;
}

vsm::result<section_handle> create_section(
	abstract_file_handle::file_handle const& backing_file,
	fs_size const max_size,
	auto&&... args)
{
	vsm::result<section_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<section_t::create_t>(
		*r,
		make_io_args<section_t::create_t>(&backing_file.native(), max_size)(vsm_forward(args)...)));
	return r;
}

} // namespace _section_b

} // namespace allio::detail
