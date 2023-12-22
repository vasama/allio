#pragma once

#include <allio/detail/handles/file.hpp>
#include <allio/detail/memory.hpp>

#include <vsm/standard.hpp>

namespace allio::detail {

enum class section_options : uint8_t
{
	backing_file                        = 1 << 0,
	backing_directory                   = 1 << 1,
};
vsm_flag_enum(section_options);

struct backing_directory_t
{
	native_handle<fs_object_t> const* backing_directory;
};

namespace section_io {

struct create_t
{
	using operation_concept = producer_t;

	struct params_type : io_flags_t
	{
		section_options options = {};
		detail::protection protection = {};
		native_handle<fs_object_t> const* backing_storage = nullptr;
		fs_size max_size = 0;

		friend void tag_invoke(set_argument_t, params_type& args, section_options const value)
		{
			args.options = value;
		}

		friend void tag_invoke(set_argument_t, params_type& args, detail::protection const value)
		{
			args.protection = value;
		}

		friend void tag_invoke(set_argument_t, params_type& args, backing_directory_t const value)
		{
			args.options |= section_options::backing_directory;
			args.backing_storage = value.backing_directory;
		}
	};

	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<create_t>,
		native_handle<Object>& h,
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

	using create_t = section_io::create_t;

	using operations = type_list_append
	<
		base_type::operations
		, create_t
	>;

	static vsm::result<void> create(
		native_handle<section_t>& h,
		io_parameters_t<section_t, create_t> const& a);
};

template<>
struct native_handle<section_t> : native_handle<section_t::base_type>
{
	detail::protection protection;
};

template<typename Traits>
[[nodiscard]] auto create_section(fs_size const max_size, auto&&... args)
{
	auto a = io_parameters_t<section_t, section_io::create_t>{};
	a.max_size = max_size;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<section_t, section_io::create_t>(a);
}

template<typename Traits>
[[nodiscard]] auto create_section(
	handle_for<detail::file_t> auto const& file,
	fs_size const max_size,
	auto&&... args)
{
	auto a = io_parameters_t<section_t, section_io::create_t>{};
	a.options = section_options::backing_file;
	a.backing_storage = &file.native();
	a.max_size = max_size;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<section_t, section_io::create_t>(a);
}

} // namespace allio::detail
