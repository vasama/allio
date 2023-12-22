#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/fs_object.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

namespace file_io {

struct tell_t
{
	using operation_concept = void;
	using params_type = no_parameters_t;
	using result_type = fs_size;

	template<object Object>
	friend vsm::result<fs_size> tag_invoke(
		blocking_io_t<tell_t>,
		native_handle<Object> const& h,
		io_parameters_t<Object, tell_t> const& a)
		requires requires { Object::tell(h, a); }
	{
		return Object::tell(h, a);
	}
};

struct seek_t
{
	using operation_concept = void;
	struct params_type
	{
		fs_size offset;
	};
	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<seek_t>,
		native_handle<Object> const& h,
		io_parameters_t<Object, seek_t> const& a)
		requires requires { Object::seek(h, a); }
	{
		return Object::seek(h, a);
	}
};

} // namespace file_io

struct file_t : fs_object_t
{
	using base_type = fs_object_t;

	using tell_t = file_io::tell_t;
	using seek_t = file_io::seek_t;
	using stream_read_t = byte_io::stream_read_t;
	using stream_write_t = byte_io::stream_write_t;
	using random_read_t = byte_io::random_read_t;
	using random_write_t = byte_io::random_write_t;

	using operations = type_list_append
	<
		base_type::operations
		, tell_t
		, seek_t
		, stream_read_t
		, stream_write_t
		, random_read_t
		, random_write_t
	>;

	static vsm::result<void> open(
		native_handle<file_t>& h,
		io_parameters_t<file_t, open_t> const& args);

	static vsm::result<fs_size> tell(
		native_handle<file_t> const& h,
		io_parameters_t<file_t, tell_t> const& args);

	static vsm::result<void> seek(
		native_handle<file_t> const& h,
		io_parameters_t<file_t, seek_t> const& args);

	static vsm::result<size_t> stream_read(
		native_handle<file_t> const& h,
		io_parameters_t<file_t, stream_read_t> const& args);

	static vsm::result<size_t> stream_write(
		native_handle<file_t> const& h,
		io_parameters_t<file_t, stream_write_t> const& args);

	static vsm::result<size_t> random_read(
		native_handle<file_t> const& h,
		io_parameters_t<file_t, random_read_t> const& args);

	static vsm::result<size_t> random_write(
		native_handle<file_t> const& h,
		io_parameters_t<file_t, random_write_t> const& args);


	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] auto read_some(fs_size const offset, read_buffer const buffer, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, random_read_t>{};
			a.buffers = buffer;
			a.offset = offset;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<random_read_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto read_some(fs_size const offset, read_buffers const buffers, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, random_read_t>{};
			a.buffers = buffers;
			a.offset = offset;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<random_read_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto write_some(fs_size const offset, write_buffer const buffer, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, random_write_t>{};
			a.buffers = buffer;
			a.offset = offset;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<random_write_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto write_some(fs_size const offset, write_buffers const buffers, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, random_write_t>{};
			a.buffers = buffers;
			a.offset = offset;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<random_write_t>(static_cast<Handle const&>(*this), a);
		}
	};
};

fs_path get_null_device_path();

template<typename Traits>
[[nodiscard]] auto open_file(fs_path const& path, auto&&... args)
{
	auto a = io_parameters_t<file_t, fs_io::open_t>{};
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<file_t, fs_io::open_t>(a);
}

template<typename Traits>
[[nodiscard]] auto open_temp_file(auto&&... args)
{
	auto a = io_parameters_t<file_t, fs_io::open_t>{};
	a.special = open_options::temporary;
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<file_t, fs_io::open_t>(a);
}

template<typename Traits>
[[nodiscard]] auto open_unique_file(
	handle_for<fs_object_t> auto const& base,
	auto&&... args)
{
	auto a = io_parameters_t<file_t, fs_io::open_t>{};
	a.special = open_options::unique_name;
	a.path.base = base.native().fs_object_t::native_type::platform_handle;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<file_t, fs_io::open_t>(a);
}

template<typename Traits>
[[nodiscard]] auto open_anonymous_file(
	handle_for<fs_object_t> auto const& base,
	auto&&... args)
{
	auto a = io_parameters_t<file_t, fs_io::open_t>{};
	a.special = open_options::anonymous;
	a.path.base = base.native().fs_object_t::native_type::platform_handle;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<file_t, fs_io::open_t>(a);
}

template<typename Traits>
[[nodiscard]] auto open_null_device(auto&&... args)
{
	auto a = io_parameters_t<file_t, fs_io::open_t>{};
	a.path = get_null_device_path();
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<file_t, fs_io::open_t>(a);
}

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/file.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/file.hpp>
#endif
