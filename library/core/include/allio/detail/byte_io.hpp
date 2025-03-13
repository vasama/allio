#pragma once

#include <allio/byte_io_buffers.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/filesystem.hpp>
#include <allio/detail/object.hpp>
#include <allio/detail/parameters.hpp>

namespace allio::detail {

struct byte_io_limits
{
	size_t max_buffer_count;
	size_t max_atomic_buffer_count;
};

struct file_offset_t
{
	fs_size offset;
};

template<vsm::any_cv_of<std::byte> T>
struct basic_buffers_t
{
	io_buffers<T> buffers;
};

namespace byte_io {

template<vsm::any_cv_of<std::byte> T>
struct stream_parameters_t
	: io_flags_t
	, deadline_t
{
	io_buffers<T> buffers;

	using io_flags_t::set_argument;
	using deadline_t::set_argument;

	void set_argument(basic_buffers_t<T> const value)
	{
		buffers = value;
	}
};

template<vsm::any_cv_of<std::byte> T>
struct random_parameters_t
	: stream_parameters_t<T>
{
	fs_size offset = {};

	using stream_parameters_t<T>::set_argument;

	void set_argument(file_offset_t const value)
	{
		offset = value.offset;
	}
};


struct stream_read_t
{
	using operation_concept = void;
	using params_type = stream_parameters_t<std::byte>;
	using result_type = size_t;

	template<object Object>
	static vsm::result<size_t> blocking_io(
		native_handle<Object> const& h,
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
	static vsm::result<size_t> blocking_io(
		native_handle<Object> const& h,
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
	static vsm::result<size_t> blocking_io(
		native_handle<Object> const& h,
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
	static vsm::result<size_t> blocking_io(
		native_handle<Object> const& h,
		io_parameters_t<Object, random_write_t> const& a)
		requires requires { Object::random_write(h, a); }
	{
		return Object::random_write(h, a);
	}
};

template<typename Handle, typename Traits>
struct common_facade
{
	[[nodiscard]] byte_io_limits limits() const
	{
		return Handle::object_type::get_byte_io_limits(static_cast<Handle const&>(*this).native());
	}
};

template<typename Handle, typename Traits>
struct stream_facade : common_facade<Handle, Traits>
{
	[[nodiscard]] auto read_some(new_read_buffers const buffers, auto&&... args) const
	{
		io_parameters_t<typename Handle::object_type, stream_read_t> a = {};
		a.buffers = buffers;
		(set_argument(a, vsm_forward(args)), ...);
		return Traits::template observe<stream_read_t>(static_cast<Handle const&>(*this), a);
	}

	[[nodiscard]] auto write_some(new_write_buffers const buffers, auto&&... args) const
	{
		io_parameters_t<typename Handle::object_type, stream_write_t> a = {};
		a.buffers = buffers;
		(set_argument(a, vsm_forward(args)), ...);
		return Traits::template observe<stream_write_t>(static_cast<Handle const&>(*this), a);
	}

	auto read(new_read_buffers const buffers, auto&&... args) const
	{
		io_parameters_t<typename Handle::object_type, stream_read_t> a = {};
		a.flags |= io_flags::greedy_byte_io;
		a.buffers = buffers;
		(set_argument(a, vsm_forward(args)), ...);
		return Traits::template observe<stream_read_t>(static_cast<Handle const&>(*this), a);
	}

	auto write(new_write_buffers const buffers, auto&&... args) const
	{
		io_parameters_t<typename Handle::object_type, stream_write_t> a = {};
		a.flags |= io_flags::greedy_byte_io;
		a.buffers = buffers;
		(set_argument(a, vsm_forward(args)), ...);
		return Traits::template observe<stream_write_t>(static_cast<Handle const&>(*this), a);
	}
};

} // namespace byte_io
} // namespace allio::detail
