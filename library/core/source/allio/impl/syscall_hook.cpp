#include <allio/impl/syscall_hook.hpp>

#include <unordered_map>

using namespace allio;
using namespace allio::detail;

namespace {

struct syscall_info
{
	syscall_type_t type;
	std::shared_ptr<syscall_interface_t> syscall;
	size_t hook_count;

	explicit syscall_info(syscall_type_t const type)
		: type(type)
	{
	}
};

struct thread_state
{
	std::unordered_map<std::string_view, syscall_info> syscalls;
};

static thread_local thread_state tls_state;

} // namespace

std::shared_ptr<syscall_interface_2> detail::get_syscall(
	std::string_view const name,
	syscall_type_t const type,
	syscall_interface_2 const& default_syscall)
{
	thread_state& state = tls_state;

	auto const it = state.syscalls.find(name);
	if (it != state.syscalls.end())
	{
		if (it->second.hook_count != 0)
		{
			vsm_assert(type == it->second.type);
			return it->second.syscall;
		}
	}

	return std::shared_ptr<syscall_interface_2>(std::shared_ptr<void>(), &default_syscall);
}

void detail::insert_syscall_hook(
	std::string_view const name,
	syscall_type_t const type,
	std::shared_ptr<syscall_interface_2> new_syscall)
{
	thread_state& state = tls_state;

	auto const it = state.syscalls.try_emplace(name, type).first;
	vsm_assert(type == it->second.type);

	it->second.syscall = std::make_shared()
}
