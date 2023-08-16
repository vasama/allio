#pragma once

#include <allio/byte_io.hpp>
#include <allio/handles/filesystem_handle2.hpp>

namespace allio {
namespace detail {

class file_handle_impl : public filesystem_handle
{
protected:
	using base_type = filesystem_handle;

public:
	using byte_io_parameters = deadline_parameters;


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		random_access_scatter_gather_list
	>;

protected:
	allio_detail_default_lifetime(file_handle_impl);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		vsm::result<size_t> read_at(file_size const offset, read_buffers const buffers, P const& args = {}) const;
		
		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		vsm::result<size_t> read_at(file_size const offset, read_buffer const buffer, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		vsm::result<size_t> write_at(file_size const offset, write_buffers const buffers, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		vsm::result<size_t> write_at(file_size const offset, write_buffer const buffer, P const& args = {}) const;
	};

	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<scatter_read_at_tag> read_at_async(file_size const offset, read_buffers const buffers, P const& args = {}) const;
		
		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<scatter_read_at_tag> read_at_async(file_size const offset, read_buffer const buffer, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<gather_write_at_tag> write_at_async(file_size const offset, write_buffers const buffers, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<gather_write_at_tag> write_at_async(file_size const offset, write_buffer const buffer, P const& args = {}) const;
	};
};



} // namespace detail

using file_handle = basic_handle<detail::file_handle_impl>;

template<typename Multiplexer>
using basic_file_handle = basic_async_handle<detail::file_handle_impl, Multiplexer>;


template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_file(path_descriptor const path, P const& args = {})
{
	vsm::result<file_handle> r(vsm::result_value);
	vsm_try_void(r->open(path, args));
	return r;
}

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_unique_file(filesystem_handle const& base, P const& args = {});

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_temporary_file(P const& args = {});

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_anonymous_file(filesystem_handle const& base, P const& args = {});

} // namespace allio
