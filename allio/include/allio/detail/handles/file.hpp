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
		blocking_io_t<Object, tell_t>,
		typename Object::native_type const& h,
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
		blocking_io_t<Object, seek_t>,
		typename Object::native_type const& h,
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
		native_type& h,
		io_parameters_t<file_t, open_t> const& args);

	static vsm::result<fs_size> tell(
		native_type const& h,
		io_parameters_t<file_t, tell_t> const& args);

	static vsm::result<void> seek(
		native_type const& h,
		io_parameters_t<file_t, seek_t> const& args);

	static vsm::result<size_t> stream_read(
		native_type const& h,
		io_parameters_t<file_t, stream_read_t> const& args);

	static vsm::result<size_t> stream_write(
		native_type const& h,
		io_parameters_t<file_t, stream_write_t> const& args);

	static vsm::result<size_t> random_read(
		native_type const& h,
		io_parameters_t<file_t, random_read_t> const& args);

	static vsm::result<size_t> random_write(
		native_type const& h,
		io_parameters_t<file_t, random_write_t> const& args);


	template<typename Handle>
	struct concrete_interface : base_type::concrete_interface<Handle>
	{
		[[nodiscard]] auto read_some(fs_size const offset, read_buffer const buffer, auto&&... args) const
		{
			return Handle::io_traits_type::template observe<random_read_t>(
				static_cast<Handle const&>(*this),
				make_args<io_parameters_t<typename Handle::object_type, random_read_t>>(
					file_offset_t{ offset },
					read_buffers_t{ buffer },
					vsm_forward(args)...));
		}

		[[nodiscard]] auto read_some(fs_size const offset, read_buffers const buffers, auto&&... args) const
		{
			return Handle::io_traits_type::template observe<random_read_t>(
				static_cast<Handle const&>(*this),
				make_args<io_parameters_t<typename Handle::object_type, random_read_t>>(
					file_offset_t{ offset },
					read_buffers_t{ buffers },
					vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(fs_size const offset, write_buffer const buffer, auto&&... args) const
		{
			return Handle::io_traits_type::template observe<random_write_t>(
				static_cast<Handle const&>(*this),
				make_args<io_parameters_t<typename Handle::object_type, random_write_t>>(
					file_offset_t{ offset },
					write_buffers_t{ buffer },
					vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(fs_size const offset, write_buffers const buffers, auto&&... args) const
		{
			return Handle::io_traits_type::template observe<random_write_t>(
				static_cast<Handle const&>(*this),
				make_args<io_parameters_t<typename Handle::object_type, random_write_t>>(
					file_offset_t{ offset },
					write_buffers_t{ buffers },
					vsm_forward(args)...));
		}
	};
};

fs_path get_null_device_path();

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/file.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/file.hpp>
#endif
