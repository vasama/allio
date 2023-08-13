#include <allio/directory_handle.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> directory_handle_base::block_open(filesystem_handle const* const base, input_path_view const path, open_parameters const& args)
{
	return block<io::filesystem_open>(static_cast<directory_handle&>(*this), args, base, path);
}

vsm::result<directory_stream_view> directory_handle_base::block_read(std::span<std::byte> const buffer, read_parameters const& args) const
{
	return block<io::directory_read>(static_cast<directory_handle const&>(*this), args, buffer);
}

vsm::result<directory_handle> detail::block_open_directory(filesystem_handle const* const base, input_path_view const path, directory_handle::open_parameters const& args)
{
	vsm::result<directory_handle> r(vsm::result_value);
	vsm_try_void(r->open(base, path, args));
	return r;
}
