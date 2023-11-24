#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/fs_object.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

struct file_t : fs_object_t
{
	using base_type = fs_object_t;

	using stream_read_t = byte_io::stream_read_t;
	using stream_write_t = byte_io::stream_write_t;
	using random_read_t = byte_io::random_read_t;
	using random_write_t = byte_io::random_write_t;

	using operations = type_list_append
	<
		base_type::operations
		, stream_read_t
		, stream_write_t
		, random_read_t
		, random_write_t
	>;

	static vsm::result<void> open(
		native_type& h,
		io_parameters_t<file_t, open_t> const& args);

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


	template<typename Handle, optional_multiplexer_handle_for<file_t> MultiplexerHandle>
	struct concrete_interface : base_type::concrete_interface<Handle, MultiplexerHandle>
	{
		[[nodiscard]] auto read_some(fs_size const offset, read_buffer const buffer, auto&&... args) const
		{
			return generic_io<random_read_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, random_read_t>(offset, buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto read_some(fs_size const offset, read_buffers const buffers, auto&&... args) const
		{
			return generic_io<random_read_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, random_read_t>(offset, buffers)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(fs_size const offset, write_buffer const buffer, auto&&... args) const
		{
			return generic_io<random_write_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, random_write_t>(offset, buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(fs_size const offset, write_buffers const buffers, auto&&... args) const
		{
			return generic_io<random_write_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, random_write_t>(offset, buffers)(vsm_forward(args)...));
		}
	};
};
using abstract_file_handle = abstract_handle<file_t>;


namespace _file_b {

using file_handle = blocking_handle<file_t>;

inline vsm::result<file_handle> open_file(fs_path const& path, auto&&... args)
{
	vsm::result<file_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<file_t, fs_io::open_t>(
		*r,
		make_io_args<file_t, fs_io::open_t>(path)(vsm_forward(args)...)));
	return r;
}

} // namespace _file_b

namespace _file_a {

template<multiplexer_handle_for<file_t> MultiplexerHandle>
using basic_file_handle = async_handle<file_t, MultiplexerHandle>;

ex::sender auto open_file(fs_path const& path, auto&&... args)
{
	return io_handle_sender<file_t, fs_io::open_t>(
		vsm_lazy(make_io_args<file_t, fs_io::open_t>(path)(vsm_forward(args)...)));
}

} // namespace _file_a

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/file.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/file.hpp>
#endif
