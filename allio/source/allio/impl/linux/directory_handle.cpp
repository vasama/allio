#include <allio/directory_handle.hpp>

#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

vsm::result<size_t> this_process::get_current_directory(output_path_ref const output)
{
	if (output.is_wide())
	{
		return vsm::unexpected(error::wide_string_not_supported);
	}

	char const* const c_string = getcwd(nullptr, 0);

	if (c_string == nullptr)
	{
		return vsm::unexpected(get_last_error());
	}

	struct cwd_deleter
	{
		void operator()(char const* const c_string) const
		{
			free(c_string);
		}
	};
	unique_ptr<char const*, cwd_deleter> const unique_c_string(c_string);

	size_t const string_size = std::strlen(c_string);
	size_t const output_size = string_size + output.is_c_string();

	vsm_try(buffer, output.resize_utf8(output_size));
	memcpy(buffer.data(), c_string, output_size);

	return string_size;
}

vsm::result<void> this_process::set_current_directory(input_path_view const path)
{
	if (output.is_wide())
	{
		return vsm::unexpected(error::wide_string_not_supported);
	}

	api_string_storage storage;
	vsm_try(c_string, make_api_c_string(storage, path));

	if (chdir(c_string) == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return {};
}

#if 0
vsm::result<directory_handle> this_process::open_current_directory()
{
	//int const fd = open(".", O_RDONLY | O_DIRECTORY, 0);
}
#endif
