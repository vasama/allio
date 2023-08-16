#include <allio/process_handle.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> process_handle_base::block_open(process_id const pid, open_parameters const& args)
{
	return block<io::process_open>(static_cast<process_handle&>(*this), args, pid);
}

vsm::result<void> process_handle_base::block_launch(filesystem_handle const* const base, input_path_view const path, launch_parameters const& args)
{
	return block<io::process_launch>(static_cast<process_handle&>(*this), args, base, path);
}

vsm::result<process_exit_code> process_handle_base::block_wait(wait_parameters const& args)
{
	return block<io::process_wait>(static_cast<process_handle&>(*this), args);
}

bool process_handle_base::check_native_handle(native_handle_type const& handle)
{
	return base_type::check_native_handle(handle);
}

void process_handle_base::set_native_handle(native_handle_type const& handle)
{
	base_type::set_native_handle(handle);
	m_pid.value = handle.pid;
	m_exit_code.value = handle.exit_code;
}

process_handle::native_handle_type process_handle_base::release_native_handle()
{
	return
	{
		base_type::release_native_handle(),
		m_pid.release(),
		m_exit_code.release(),
	};
}


vsm::result<process_handle> allio::open_process(process_id const pid, process_handle::open_parameters::interface const& args)
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(r->open(pid, args));
	return r;
}

vsm::result<process_handle> allio::launch_process(input_path_view const path, process_handle::launch_parameters::interface const& args)
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(r->launch(path, args));
	return r;
}
