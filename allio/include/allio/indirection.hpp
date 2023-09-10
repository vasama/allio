#pragma once

#include <vsm/utility.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <memory>

namespace allio {

#if 0
struct unwrap_cpo
{
	template<typename T>
	friend T& tag_invoke(unwrap_cpo, T& object)
	{
		return object;
	}

	decltype(auto) vsm_static_operator_invoke(auto&& object)
	{
		return vsm::tag_invoke(unwrap_cpo(), vsm_forward(object));
	}
};
inline constexpr unwrap_cpo unwrap = {};

template<typename T>
using unwrap_t = std::remove_reference_t<decltype(unwrap(vsm_declval(T)))>;
#endif


struct borrow_cpo
{
	template<typename T>
	friend T& tag_invoke(borrow_cpo, T& object)
	{
		return object;
	}

	decltype(auto) vsm_static_operator_invoke(auto&& object)
	{
		return vsm::tag_invoke(borrow_cpo(), vsm_forward(object));
	}
};
inline constexpr borrow_cpo borrow = {};

template<typename T>
using borrow_t = std::remove_reference_t<decltype(borrow(vsm_declval(T)))>;


template<typename Pointer>
class basic_pointer_indirect
{
	Pointer m_pointer;

public:
	using element_type = typename std::pointer_traits<Pointer>::element_type;

	basic_pointer_indirect() = default;

	explicit basic_pointer_indirect(Pointer pointer)
		: m_pointer(vsm_move(pointer))
	{
	}

	explicit operator bool() const
	{
		return static_cast<bool>(m_pointer);
	}

	template<typename CPO, typename... Args>
	friend auto tag_invoke(CPO cpo, basic_pointer_indirect const& h, Args&&... args)
		-> decltype(cpo(*h.m_pointer, vsm_forward(args)...))
	{
		return cpo(*h.m_pointer, vsm_forward(args)...);
	}

	friend basic_pointer_indirect<element_type*> tag_invoke(borrow_cpo, basic_pointer_indirect const& pointer)
	{
		return basic_pointer_indirect<element_type*>(std::to_address(pointer.m_pointer));
	}
};

template<typename T>
using borrow_indirect = basic_pointer_indirect<T*>;

template<typename T>
using shared_indirect = basic_pointer_indirect<std::shared_ptr<T>>;

#if 0
template<auto& Object>
class global_indirect
{
public:
	using element_type = std::remove_reference_t<decltype(Object)>;

	template<typename CPO, typename... Args>
	friend auto tag_invoke(CPO cpo, global_indirect, Args&&... args)
		-> decltype(cpo(Object, vsm_forward(args)...))
	{
	}

	friend global_indirect tag_invoke(borrow_cpo, global_indirect)
	{
		return {};
	}
};
#endif

} // namespace allio
