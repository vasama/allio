#pragma once

#if !allio_config_syscall_hook
#	error syscall_hook.hpp included with invalid configuration.
#endif

#include <vsm/platform.h>

namespace allio::detail {

template<typename R, typename... Ps>
class new_syscall_hook;

template<auto const& Syscall>
struct new_syscall_default_t
{
	explicit new_syscall_default_t() = default;
};

template<typename R, typename... Ps>
class new_syscall_interface
{
public:
	[[nodiscard]] R invoke(Ps... args) const = 0;

protected:
	new_syscall_interface() = default;
	new_syscall_interface(new_syscall_interface const&) = default;
	new_syscall_interface& operator=(new_syscall_interface const&) = default;
	~new_syscall_interface() = default;
};

template<typename R, typename... Ps>
class new_syscall
{
	template<auto const& Syscall>
	struct default_syscall : new_syscall_interface<R, Ps...>
	{
		[[nodiscard]] R invoke(Ps const... args) const override
		{
			return Syscall(args...);
		}
	};

	template<auto const& Syscall>
	static new_syscall_interface<R, Ps...> const* get_default_syscall()
	{
		static constexpr default_syscall<Syscall> syscall;
		return &syscall;
	}

	new_syscall_interface<R, Ps...> const* m_interface;

public:
	template<auto const& Syscall>
	new_syscall(new_syscall_default_t<Syscall>)
		: m_interface(get_default_syscall<Syscall>())
	{
	}

	new_syscall(new_syscall const&) = delete;
	new_syscall& operator=(new_syscall const&) = delete;

	[[nodiscard]] R operator()(Ps const... args) const
	{
		return m_interface->invoke(args...);
	}

private:
	friend new_syscall_hook<R, Ps...>;
};

template<typename R, typename... Ps>
class new_syscall_hook : protected new_syscall_interface<R, Ps...>
{
	new_syscall<R, Ps...>& m_syscall;
	new_syscall_interface<R, Ps...> const* m_next_interface;

public:
	new_syscall_hook(new_syscall_hook const&) = delete;
	new_syscall_hook& operator=(new_syscall_hook const&) = delete;

protected:
	new_syscall_hook(new_syscall<R, Ps...>& syscall)
		: m_syscall(syscall)
		, m_next_interface(std::exchange(syscall.m_interface, this))
	{
	}

	~new_syscall_hook()
	{
		m_syscall.m_interface = m_next_interface;
	}

	[[nodiscard]] R invoke_next(Ps const... args) const
	{
		return m_next_interface->invoke(args...);
	}
};

template<typename Syscall>
struct new_syscall_for_1;

template<typename Syscall>
struct new_syscall_for_1<Syscall* const&>
{
	using type = typename new_syscall_for_1<Syscall&>::type;
};

template<typename R, typename... Ps>
struct new_syscall_for_1<R(&)(Ps...)>
{
	using type = new_syscall<R, Ps...>;
};

#if vsm_os_win32 && vsm_arch_x86_32
template<typename R, typename... Ps>
struct new_syscall_for_1<R(__stdcall&)(Ps...)>
{
	using type = new_syscall<R, Ps...>;
};
#endif

template<auto const& Syscall>
using new_syscall_for = new_syscall<decltype(Syscall)>;

template<auto const& Syscall>
new_syscall_for<Syscall> syscall = new_syscall_default_t<Syscall>();


#if 0
using syscall_type_t = void const*;

template<typename Signature>
inline char const syscall_type_v[1] = {};


class syscall_interface_2
{
protected:
	syscall_interface_2() = default;
	syscall_interface_2(syscall_interface_2 const&) = default;
	syscall_interface_2& operator=(syscall_interface_2 const&) = default;
	~syscall_interface_2() = default;
};

std::shared_ptr<syscall_interface_2> get_syscall(
	std::string_view name,
	syscall_type_t type,
	syscall_interface_2 const& default_syscall);

void insert_syscall_hook(
	std::string_view name,
	syscall_type_t type,
	std::shared_ptr<syscall_interface_2> new_syscall);

void remove_syscall_hook(
	std::string_view name,
	syscall_type_t type);


template<typename R, typename... Ps>
class syscall_interface_1 : public syscall_interface_2
{
public:
	[[nodiscard]] virtual R invoke(Ps... args) const = 0;

protected:
	syscall_interface_1() = default;
	syscall_interface_1(syscall_interface_1 const&) = default;
	syscall_interface_1& operator=(syscall_interface_1 const&) = default;
	~syscall_interface_1() = default;
};

template<typename R, typename... Ps>
class syscall_interface : public syscall_interface_1<R, Ps...>
{
	std::shared_ptr<syscall_interface_1<R, Ps...> const> m_next;

protected:
	syscall_interface() = default;
	syscall_interface(syscall_interface const&) = default;
	syscall_interface& operator=(syscall_interface const&) = default;
	~syscall_interface() = default;

	[[nodiscard]] R invoke_next(Ps const... args) const
	{
		return m_next->invoke(args...);
	}
};


template<typename R, typename... Ps>
class syscall_wrapper
{
	using interface_type = syscall_interface_1<R, Ps...>;

	template<auto const& Syscall>
	struct default_syscall : interface_type
	{
		R invoke(Ps const... args) const override
		{
			return Syscall(args...);
		}

		static syscall_interface_2 const& get()
		{
			static constexpr default_syscall object;
			return object;
		}
	};

	std::shared_ptr<interface_type const> m_syscall;

public:
	[[nodiscard]] R operator()(Ps const... args) const
	{
		return m_syscall->invoke(args...);
	}

	template<auto const& Syscall>
	[[nodiscard]] static syscall_wrapper get(std::string_view const name)
	{
		return syscall_wrapper(get_syscall(
			name,
			syscall_type_v<R(Ps...)>,
			default_syscall<Syscall>::get()));
	}

private:
	explicit syscall_wrapper(std::shared_ptr<syscall_interface_2 const> syscall)
		: m_syscall(std::static_pointer_cast<interface_type const>(vsm_move(syscall)))
	{
	}
};

template<typename Signature>
struct syscall_wrapper_1;

template<typename R, typename... Ps>
struct syscall_wrapper_1<R(Ps...)>
{
	using type = syscall_wrapper<R, Ps...>;
};

#if vsm_os_win32 && vsm_x86_32
template<typename R, typename... Ps>
struct syscall_wrapper_1<R __stdcall(Ps...)>
{
	using type = syscall_wrapper<R, Ps...>;
};
#endif

template<typename Signature>
struct syscall_wrapper_1<Signature const&>
{
	using type = typename syscall_wrapper_1<Signature>::type;
};

template<typename Signature>
struct syscall_wrapper_1<Signature* const&>
{
	using type = typename syscall_wrapper_1<Signature>::type;
};

template<auto const& Syscall>
using syscall_wrapper_for = typename syscall_wrapper_1<decltype(Syscall)>::type;


template<auto const& Syscall>
syscall_wrapper_for<Syscall> wrap_syscall(std::string_view const name)
{
	return syscall_wrapper_for<Syscall>::get<Syscall>(name);
}
#endif

#ifndef vsm_compiler_preview
#	define allio_syscall(_syscall) ::allio::detail::syscall<_syscall>
#endif

} // namespace allio::detail
