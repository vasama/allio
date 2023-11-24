#pragma once

#include <allio/byte_io_buffers.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/filesystem.hpp>
#include <allio/detail/object.hpp>

namespace allio::detail {

template<vsm::any_cv_of<std::byte> T>
struct basic_buffers_t
{
	basic_buffers_storage<T> buffers;
};

using read_buffers_t = basic_buffers_t<std::byte>;
using write_buffers_t = basic_buffers_t<std::byte const>;


namespace byte_io {

using optional_parameters = deadline_t;


template<vsm::any_cv_of<std::byte> T>
using stream_required_parameters_t = basic_buffers_t<T>;

template<vsm::any_cv_of<std::byte> T>
using stream_parameters_t = _io_parameters<stream_required_parameters_t<T>, optional_parameters>;


template<vsm::any_cv_of<std::byte> T>
struct random_required_parameters_t
{
	fs_size offset;
	basic_buffers_storage<T> buffers;
};

template<vsm::any_cv_of<std::byte> T>
using random_parameters_t = _io_parameters<random_required_parameters_t<T>, optional_parameters>;


struct stream_read_t
{
	using operation_concept = void;
	using required_params_type = stream_required_parameters_t<std::byte>;
	using optional_params_type = optional_parameters;
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
	using required_params_type = stream_required_parameters_t<std::byte const>;
	using optional_params_type = optional_parameters;
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
	using required_params_type = random_required_parameters_t<std::byte>;
	using optional_params_type = optional_parameters;
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
	using required_params_type = random_required_parameters_t<std::byte const>;
	using optional_params_type = optional_parameters;
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

} // namespace byte_io
} // namespace allio::detail
