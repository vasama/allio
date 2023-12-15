#pragma once

#include <allio/byte_io_buffers.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/filesystem.hpp>
#include <allio/detail/object.hpp>
#include <allio/detail/parameters.hpp>

namespace allio::detail {

template<vsm::any_cv_of<std::byte> T>
struct basic_buffers_t
{
	basic_buffers_storage<T> buffers;
};

using read_buffers_t = basic_buffers_t<std::byte>;
using write_buffers_t = basic_buffers_t<std::byte const>;

struct file_offset_t
{
	fs_size offset;
};

namespace byte_io {

template<vsm::any_cv_of<std::byte> T>
struct stream_parameters_t : io_flags_t, deadline_t
{
	basic_buffers_storage<T> buffers;

	friend void tag_invoke(set_argument_t, stream_parameters_t& args, basic_buffers_t<T> const value)
	{
		args.buffers = value.buffers;
	}
};

template<vsm::any_cv_of<std::byte> T>
struct random_parameters_t : stream_parameters_t<T>
{
	fs_size offset = {};

	friend void tag_invoke(set_argument_t, random_parameters_t& args, file_offset_t const value)
	{
		args.offset = value.offset;
	}
};


struct stream_read_t
{
	using operation_concept = void;
	using params_type = stream_parameters_t<std::byte>;
	using result_type = size_t;

	template<object Object>
	friend vsm::result<size_t> tag_invoke(
		blocking_io_t<Object, stream_read_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, stream_read_t> const& a)
		requires requires { Object::stream_read(h, a); }
	{
		return Object::stream_read(h, a);
	}
};

struct stream_write_t
{
	using operation_concept = void;
	using params_type = stream_parameters_t<std::byte const>;
	using result_type = size_t;

	template<object Object>
	friend vsm::result<size_t> tag_invoke(
		blocking_io_t<Object, stream_write_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, stream_write_t> const& a)
		requires requires { Object::stream_write(h, a); }
	{
		return Object::stream_write(h, a);
	}
};

struct random_read_t
{
	using operation_concept = void;
	using params_type = random_parameters_t<std::byte>;
	using result_type = size_t;

	template<object Object>
	friend vsm::result<size_t> tag_invoke(
		blocking_io_t<Object, random_read_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, random_read_t> const& a)
		requires requires { Object::random_read(h, a); }
	{
		return Object::random_read(h, a);
	}
};

struct random_write_t
{
	using operation_concept = void;
	using params_type = random_parameters_t<std::byte const>;
	using result_type = size_t;

	template<object Object>
	friend vsm::result<size_t> tag_invoke(
		blocking_io_t<Object, random_write_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, random_write_t> const& a)
		requires requires { Object::random_write(h, a); }
	{
		return Object::random_write(h, a);
	}
};

template<typename Handle>
struct stream_interface
{
	[[nodiscard]] auto read_some(read_buffer const buffer, auto&&... args) const
	{
		io_parameters_t<typename Handle::object_type, stream_read_t> a = {};
		a.buffers = buffer;
		(set_argument(a, vsm_forward(args)), ...);
		return Handle::io_traits_type::template observe<stream_read_t>(
			static_cast<Handle const&>(*this),
			a);
	}

	[[nodiscard]] auto read_some(read_buffers const buffers, auto&&... args) const
	{
		io_parameters_t<typename Handle::object_type, stream_read_t> a = {};
		a.buffers = buffers;
		(set_argument(a, vsm_forward(args)), ...);
		return Handle::io_traits_type::template observe<stream_read_t>(
			static_cast<Handle const&>(*this),
			a);
	}

	[[nodiscard]] auto write_some(write_buffer const buffer, auto&&... args) const
	{
		io_parameters_t<typename Handle::object_type, stream_write_t> a = {};
		a.buffers = buffer;
		(set_argument(a, vsm_forward(args)), ...);
		return Handle::io_traits_type::template observe<stream_write_t>(
			static_cast<Handle const&>(*this),
			a);
	}

	[[nodiscard]] auto write_some(write_buffers const buffers, auto&&... args) const
	{
		io_parameters_t<typename Handle::object_type, stream_write_t> a = {};
		a.buffers = buffers;
		(set_argument(a, vsm_forward(args)), ...);
		return Handle::io_traits_type::template observe<stream_write_t>(
			static_cast<Handle const&>(*this),
			a);
	}
};

} // namespace byte_io
} // namespace allio::detail
