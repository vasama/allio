#pragma once

#include <allio/async_fwd.hpp>
#include <allio/byte_io.hpp>
#include <allio/common_socket_handle.hpp>
#include <allio/detail/api.hpp>

namespace allio {

struct packet_read_result
{
	size_t packet_size;
	network_address address;
};

struct packet_read_descriptor
{
	read_buffers buffers;
	packet_read_result* result;
};

struct packet_write_descriptor
{
	write_buffers buffers;
	network_address const* address;
};

using packet_read_descriptors = std::span<packet_read_descriptor const>;
using packet_write_descriptors = std::span<packet_write_descriptor const>;


namespace io {

struct packet_scatter_read;
struct packet_gather_write;

using packet_scatter_gather = type_list<
	packet_scatter_read,
	packet_gather_write
>;

} // namespace io

namespace detail {

class packet_socket_handle_base : public common_socket_handle
{
	using final_handle_type = final_handle<packet_socket_handle_base>;

public:
	using base_type = common_socket_handle;

	using async_operations = type_list_cat<
		common_socket_handle::async_operations,
		io::packet_scatter_gather
	>;


	using packet_io_parameters = basic_parameters;


	constexpr packet_socket_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	vsm::result<packet_read_result> read(read_buffer const buffer, P const& args = {})
	{
		vsm::result<packet_read_result> r(vsm::result_value);
		packet_read_descriptor const descriptor = { read_buffers(&buffer, 1), &*r };
		vsm_try_discard(block_read(block_read(packet_read_descriptors(&descriptor, 1), args)));
		return r;
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	vsm::result<packet_read_result> read(read_buffers const buffers, P const& args = {})
	{
		vsm::result<packet_read_result> r(vsm::result_value);
		packet_read_descriptor const descriptor = { buffers, &*r };
		vsm_try_discard(block_read(block_read(packet_read_descriptors(&descriptor, 1), args)));
		return r;
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	vsm::result<void> read(packet_read_descriptor const descriptor, P const& args = {})
	{
		return discard_value(block_read(packet_read_descriptors(&descriptor, 1), args));
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	vsm::result<size_t> read(packet_read_descriptors const descriptors, P const& args = {})
	{
		return block_read(descriptors, args);
	}


	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	vsm::result<void> write(network_address const& address, write_buffer const buffer, P const& args)
	{
		packet_write_descriptor const descriptor = { write_buffers(&buffer, 1), address };
		return discard_value(block_write(packet_write_descriptors(&descriptor, 1), args));
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	vsm::result<void> write(network_address const& address, write_buffers const buffers, P const& args)
	{
		packet_write_descriptor const descriptor = { buffers, address };
		return discard_value(block_write(packet_write_descriptors(&descriptor, 1), args));
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	vsm::result<void> write(packet_write_descriptor const descriptor, P const& args = {})
	{
		return discard_value(block_write(packet_write_descriptors(&descriptor, 1), args));
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	vsm::result<size_t> write(packet_write_descriptors const descriptors, P const& args = {})
	{
		return block_write(descriptors, args);
	}

private:
	vsm::result<size_t> block_read(packet_read_descriptors descriptors, packet_io_parameters const& args);
	vsm::result<size_t> block_write(packet_write_descriptors descriptors, packet_io_parameters const& args);

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::socket_create> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::packet_scatter_read> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::packet_gather_write> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

} // namespace detail

using packet_socket_handle = final_handle<detail::packet_socket_handle_base>;


template<>
struct io::parameters<io::packet_scatter_read>
	: packet_socket_handle::packet_io_parameters
{
	using handle_type = packet_socket_handle const;
	using result_type = size_t;

	packet_read_descriptors descriptors;
};

template<>
struct io::parameters<io::packet_gather_write>
	: packet_socket_handle::packet_io_parameters
{
	using handle_type = packet_socket_handle const;
	using result_type = size_t;

	packet_write_descriptors descriptors;
};

allio_detail_api extern allio_handle_implementation(packet_socket_handle);

} // namespace allio
