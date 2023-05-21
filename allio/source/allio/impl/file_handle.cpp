#include <allio/file_handle.hpp>

#include <allio/path_literal.hpp>

#include "unique_name.hpp"

using namespace allio;

vsm::result<void> detail::file_handle_base::block_open(filesystem_handle const* const base, input_path_view const path, open_parameters const& args)
{
	return block<io::filesystem_open>(static_cast<file_handle&>(*this), args, base, path);
}

vsm::result<size_t> detail::file_handle_base::block_read_at(file_size const offset, read_buffers const buffers, byte_io_parameters const& args) const
{
	return block<io::scatter_read_at>(static_cast<file_handle const&>(*this), args, offset, buffers);
}

vsm::result<size_t> detail::file_handle_base::block_write_at(file_size const offset, write_buffers const buffers, byte_io_parameters const& args) const
{
	return block<io::gather_write_at>(static_cast<file_handle const&>(*this), args, offset, buffers);
}


vsm::result<file_handle> detail::block_open_file(filesystem_handle const* const base, input_path_view const path, file_handle::open_parameters const& args)
{
	vsm::result<file_handle> file_result(vsm::result_value);
	vsm_try_void(file_result->open(base, path, args));
	return file_result;
}

vsm::result<file_handle> detail::block_open_unique_file(filesystem_handle const* const base, file_handle_base::open_parameters const& args)
{
	std::error_condition const file_exists = std::errc::file_exists;

	vsm::result<file_handle> file_result(vsm::result_value);
	auto name = unique_name(std::integral_constant<size_t, 32>(), allio_path(".random"));

	while (true)
	{
		if (auto const r = file_result->open(base, name.generate(), args))
		{
			return file_result;
		}
		else if (auto const& e = r.error(); e.default_error_condition() != file_exists)
		{
			return std::unexpected(e);
		}
	}
}

vsm::result<file_handle> detail::block_open_temporary_file(file_handle_base::open_parameters const& args)
{
	return std::unexpected(error::unsupported_operation);
}

vsm::result<file_handle> detail::block_open_anonymous_file(filesystem_handle const* const base, file_handle_base::open_parameters const& args)
{
	return std::unexpected(error::unsupported_operation);
}
