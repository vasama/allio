#include <allio/process_handle.hpp>

#include <allio/impl/win32/platform_handle.hpp>

namespace allio {

struct detail::process_handle_base::implementation : base_type::implementation
{
	allio_handle_implementation_flags
	(
		pseudo_handle,
	);
};

namespace win32 {

struct process_info
{
	unique_handle handle;
	process_id pid;

	process_info(HANDLE const handle, process_id const pid)
		: handle(handle)
		, pid(pid)
	{
	}
};

vsm::result<unique_handle> open_process(process_id pid, process_handle::open_parameters const& args);
vsm::result<process_info> launch_process(input_path_view path, process_handle::launch_parameters const& args);

} // namespace win32
} // namespace allio
