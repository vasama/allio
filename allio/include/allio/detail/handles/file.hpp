#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/fs_object.hpp>

namespace allio::detail {

struct file_t : fs_object_t
{
	using base_type = fs_object_t;

	using operations = type_list_cat<
		base_type::operations
		//TODO: file operations
	>;
};

#if 0
class _file_handle : public filesystem_handle
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
	allio_detail_default_lifetime(_file_handle);

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

	template<typename M, typename H>
	struct async_interface : base_type::async_interface<M, H>
	{
		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<M, H, scatter_read_at_t> read_at_async(file_size const offset, read_buffers const buffers, P const& args = {}) const;
		
		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<M, H, scatter_read_at_t> read_at_async(file_size const offset, read_buffer const buffer, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<M, H, gather_write_at_t> write_at_async(file_size const offset, write_buffers const buffers, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<M, H, gather_write_at_t> write_at_async(file_size const offset, write_buffer const buffer, P const& args = {}) const;
	};
};
#endif

} // namespace allio::detail
