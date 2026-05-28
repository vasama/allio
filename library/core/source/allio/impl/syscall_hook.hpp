#pragma once

#if !allio_config_syscall_hook
#	error syscall_hook_base.hpp included with invalid configuration.
#endif

#include <vsm/platform.h>

#include <utility>

namespace allio::detail {

template<typename R, typename... Ps>
class syscall_hook_base;

template<auto const& Syscall>
struct default_syscall_t
{
	explicit default_syscall_t() = default;
};

template<typename R, typename... Ps>
class syscall_interface
{
public:
	[[nodiscard]] R invoke(Ps... args) const = 0;

protected:
	syscall_interface() = default;
	syscall_interface(syscall_interface const&) = default;
	syscall_interface& operator=(syscall_interface const&) = default;
	~syscall_interface() = default;
};

template<typename R, typename... Ps>
class syscall_wrapper
{
	template<auto const& Syscall>
	struct default_syscall : syscall_interface<R, Ps...>
	{
		[[nodiscard]] R invoke(Ps const... args) const override
		{
			return Syscall(args...);
		}
	};

	template<auto const& Syscall>
	static syscall_interface<R, Ps...> const* get_default_syscall()
	{
		static constexpr default_syscall<Syscall> syscall;
		return &syscall;
	}

	syscall_interface<R, Ps...> const* m_interface;

public:
	template<auto const& Syscall>
	syscall_wrapper(default_syscall_t<Syscall>)
		: m_interface(get_default_syscall<Syscall>())
	{
	}

	syscall_wrapper(syscall_wrapper const&) = delete;
	syscall_wrapper& operator=(syscall_wrapper const&) = delete;

	[[nodiscard]] R operator()(Ps const... args) const
	{
		return m_interface->invoke(args...);
	}

private:
	friend syscall_hook_base<R, Ps...>;
};

template<typename R, typename... Ps>
class syscall_hook_base : protected syscall_interface<R, Ps...>
{
	syscall_wrapper<R, Ps...>& m_syscall;
	syscall_interface<R, Ps...> const* m_next_interface;

public:
	syscall_hook_base(syscall_hook_base const&) = delete;
	syscall_hook_base& operator=(syscall_hook_base const&) = delete;

protected:
	syscall_hook_base(syscall_wrapper<R, Ps...>& syscall)
		: m_syscall(syscall)
		, m_next_interface(std::exchange(syscall.m_interface, this))
	{
	}

	~syscall_hook_base()
	{
		m_syscall.m_interface = m_next_interface;
	}

	[[nodiscard]] R invoke_next(Ps const... args) const
	{
		return m_next_interface->invoke(args...);
	}
};

template<typename Syscall>
struct syscall_wrapper_for_1;

template<typename Syscall>
struct syscall_wrapper_for_1<Syscall* const&>
{
	using type = typename syscall_wrapper_for_1<Syscall&>::type;
};

template<typename R, typename... Ps>
struct syscall_wrapper_for_1<R(&)(Ps...)>
{
	using type = syscall_wrapper<R, Ps...>;
};

#if vsm_os_win32 && vsm_arch_x86_32
template<typename R, typename... Ps>
struct syscall_wrapper_for_1<R(__stdcall&)(Ps...)>
{
	using type = syscall_wrapper<R, Ps...>;
};
#endif

template<auto const& Syscall>
using syscall_wrapper_for = syscall_wrapper<decltype(Syscall)>;

template<auto const& Syscall>
syscall_wrapper_for<Syscall> wrap_syscall = default_syscall_t<Syscall>();

template<auto const& Syscall>
class syscall_hook : public syscall_hook_base<void>
{
protected:
	syscall_hook()
		: syscall_hook_base<void>(wrap_syscall<Syscall>)
	{
	}
};

#ifndef vsm_compiler_preview
#	define allio_syscall(_syscall) (::allio::detail::wrap_syscall<_syscall>)
#endif

} // namespace allio::detail
