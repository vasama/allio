#include <allio/process_handle.hpp>

#include <allio/impl/object_transaction.hpp>

using namespace allio;

result<void> detail::process_handle_base::block_open(process_id const pid, open_parameters const& args)
{
	return block<io::process_open>(static_cast<process_handle&>(*this), args, pid);
}

result<void> detail::process_handle_base::block_launch(input_path_view const path, launch_parameters const& args)
{
	return block<io::process_launch>(static_cast<process_handle&>(*this), args, path);
}

result<process_exit_code> detail::process_handle_base::block_wait(wait_parameters const& args)
{
	return block<io::process_wait>(static_cast<process_handle&>(*this), args);
}

result<void> detail::process_handle_base::set_native_handle(native_handle_type const handle)
{
	object_transaction pid_transaction(m_pid.value, handle.pid);
	object_transaction exit_code_transaction(m_exit_code.value, handle.exit_code);
	allio_TRYV(base_type::set_native_handle(handle));
	pid_transaction.commit();
	exit_code_transaction.commit();

	return {};
}

result<process_handle::native_handle_type> detail::process_handle_base::release_native_handle()
{
	allio_TRY(base_handle, base_type::release_native_handle());

	return result<native_handle_type>
	{
		result_value,
		base_handle,
		std::exchange(m_pid.value, 0),
		std::exchange(m_exit_code.value, 0),
	};
}


result<process_handle> allio::open_process(process_id const pid, process_handle::open_parameters::interface const& args)
{
	result<process_handle> r(result_value);
	allio_TRYV(r->open(pid, args));
	return r;
}

result<process_handle> allio::launch_process(input_path_view const path, process_handle::launch_parameters::interface const& args)
{
	result<process_handle> r(result_value);
	allio_TRYV(r->launch(path, args));
	return r;
}
