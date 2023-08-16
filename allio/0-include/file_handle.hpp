#pragma once

#include <allio/async_fwd.hpp>
#include <allio/detail/api.hpp>
#include <allio/byte_io.hpp>
#include <allio/filesystem_handle.hpp>
#include <allio/multiplexer.hpp>

namespace allio {
namespace detail {

class file_handle_base : public filesystem_handle
{
	using final_handle_type = final_handle<file_handle_base>;

public:
	using base_type = filesystem_handle;

	using async_operations = type_list_cat<
		base_type::async_operations,
		io::random_access_scatter_gather
	>;


	using open_parameters = base_type::open_parameters;
	using byte_io_parameters = basic_parameters;


	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(input_path_view const path, P const& args = {})
	{
		return block_open(nullptr, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(filesystem_handle const& base, input_path_view const path, P const& args = {})
	{
		return block_open(&base, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(filesystem_handle const* const base, input_path_view const path, P const& args = {})
	{
		return block_open(base, path, args);
	}

	template<parameters<file_handle_base::open_parameters> P = open_parameters::interface>
	basic_sender<io::filesystem_open> open_async(input_path_view path, P const& args = {});

	template<parameters<file_handle_base::open_parameters> P = open_parameters::interface>
	basic_sender<io::filesystem_open> open_async(filesystem_handle const& base, input_path_view path, P const& args = {});

	template<parameters<file_handle_base::open_parameters> P = open_parameters::interface>
	basic_sender<io::filesystem_open> open_async(filesystem_handle const* base, input_path_view path, P const& args = {});


	template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
	vsm::result<size_t> read_at(file_size const offset, read_buffers const buffers, P const& args = {}) const
	{
		return block_read_at(offset, buffers, args);
	}

	template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
	vsm::result<size_t> read_at(file_size const offset, read_buffer const buffer, P const& args = {}) const
	{
		return block_read_at(offset, read_buffers(&buffer, 1), args);
	}

	template<parameters<file_handle_base::byte_io_parameters> P = byte_io_parameters::interface>
	basic_sender<io::scatter_read_at> read_at_async(file_size offset, read_buffer buffer, P const& args = {}) const;

	template<parameters<file_handle_base::byte_io_parameters> P = byte_io_parameters::interface>
	basic_sender<io::scatter_read_at> read_at_async(file_size offset, read_buffers buffers, P const& args = {}) const;


	template<parameters<file_handle_base::byte_io_parameters> P = byte_io_parameters::interface>
	vsm::result<size_t> write_at(file_size const offset, write_buffers const buffers, P const& args = {}) const
	{
		return block_write_at(offset, buffers, args);
	}

	template<parameters<file_handle_base::byte_io_parameters> P = byte_io_parameters::interface>
	vsm::result<size_t> write_at(file_size const offset, write_buffer const buffer, P const& args = {}) const
	{
		return block_write_at(offset, write_buffers(&buffer, 1), args);
	}

	template<parameters<file_handle_base::byte_io_parameters> P = byte_io_parameters::interface>
	basic_sender<io::gather_write_at> write_at_async(file_size offset, write_buffer buffer, P const& args = {}) const;

	template<parameters<file_handle_base::byte_io_parameters> P = byte_io_parameters::interface>
	basic_sender<io::gather_write_at> write_at_async(file_size offset, write_buffers buffers, P const& args = {}) const;

protected:
	using base_type::base_type;

private:
	vsm::result<void> block_open(filesystem_handle const* base, input_path_view path, open_parameters const& args);
	vsm::result<size_t> block_read_at(file_size offset, read_buffers buffers, byte_io_parameters const& args) const;
	vsm::result<size_t> block_write_at(file_size offset, write_buffers buffers, byte_io_parameters const& args) const;

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::filesystem_open> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::scatter_read_at> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::gather_write_at> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

vsm::result<final_handle<file_handle_base>> block_open_file(filesystem_handle const* base, input_path_view path, file_handle_base::open_parameters const& args);
vsm::result<final_handle<file_handle_base>> block_open_unique_file(filesystem_handle const* base, file_handle_base::open_parameters const& args);
vsm::result<final_handle<file_handle_base>> block_open_temporary_file(file_handle_base::open_parameters const& args);
vsm::result<final_handle<file_handle_base>> block_open_anonymous_file(filesystem_handle const* base, file_handle_base::open_parameters const& args);

} // namespace detail

using file_handle = final_handle<detail::file_handle_base>;

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_file(input_path_view const path, P const& args = {})
{
	return detail::block_open_file(nullptr, path, args);
}

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_file(filesystem_handle const& base, input_path_view const path, P const& args = {})
{
	return detail::block_open_file(&base, path, args);
}

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_unique_file(filesystem_handle const& base, P const& args = {})
{
	return detail::block_open_unique_file(base, args);
}

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_temporary_file(P const& args = {})
{
	return detail::block_open_temporary_file(args);
}

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_anonymous_file(filesystem_handle const& base, P const& args = {})
{
	return detail::block_open_anonymous_file(&base, args);
}

allio_detail_api extern allio_handle_implementation(file_handle);

} // namespace allio
