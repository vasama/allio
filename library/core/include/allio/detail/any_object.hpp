#pragma once

#include <allio/detail/handle.hpp>
#include <allio/detail/new.h>
#include <allio/detail/type_list.hpp>

namespace allio::detail {

template<size_t MaxPointerCount>
class any_object_storage
{
	static constexpr size_t size = MaxPointerCount * sizeof(void*);
	static constexpr size_t alignment = alignof(void*);

	template<typename T>
	static constexpr bool requires_dynamic_storage =
		sizeof(T) > size || alignof(T) > alignment || !std::is_trivially_copyable_v<T>;

	union
	{
		void* m_storage_ptr;
		alignas(alignment) unsigned char m_storage[size];
	};

public:
	template<typename T>
	class storage_ptr
	{
		void* m_storage_ptr = nullptr;

	public:
		storage_ptr() = default;

		explicit storage_ptr(void* const storage_ptr)
			: m_storage_ptr(storage_ptr)
		{
		}

		storage_ptr(storage_ptr const&) = delete;
		storage_ptr& operator=(storage_ptr const&) = delete;

		~storage_ptr()
		{
			if (m_storage_ptr != nullptr)
			{
				allio_release_storage(
					m_storage_ptr,
					sizeof(T),
					alignof(T),
					allio_allocation_strategy_generic);
			}
		}

	private:
		friend any_object_storage;
	};

	template<vsm::non_cvref T>
	[[nodiscard]] static vsm::result<storage_ptr<T>> allocate()
	{
		if constexpr (requires_dynamic_storage<T>)
		{
			auto const storage = allio_acquire_storage(
				sizeof(T),
				sizeof(T),
				alignof(T),
				allio_allocation_strategy_generic);

			if (storage.storage == nullptr)
			{
				return vsm::unexpected(error::not_enough_memory);
			}

			return vsm::result<storage_ptr<T>>(vsm::result_value, storage.storage);
		}
		else
		{
			return {};
		}
	}

	template<vsm::non_cvref T, typename... Args>
		requires std::constructible_from<T, Args...>
	T* construct_at(storage_ptr<T>& storage, Args&&... args)
	{
		if constexpr (requires_dynamic_storage<T>)
		{
			T* const ptr = ::new (storage.m_storage_ptr) T(vsm_forward(args)...);

			m_storage_ptr = ptr;
			storage.m_storage_ptr = nullptr;

			return ptr;
		}
		else
		{
			return ::new (m_storage) T(vsm_forward(args)...);
		}
	}

	template<vsm::non_cvref T, typename... Args>
		requires std::constructible_from<T, Args...>
	[[nodiscard]] vsm::result<T*> construct(Args&&... args)
	{
		if constexpr (requires_dynamic_storage<T>)
		{
			auto const storage = allio_acquire_storage(
				sizeof(T),
				sizeof(T),
				alignof(T),
				allio_allocation_strategy_generic);

			if (storage.storage == nullptr)
			{
				return vsm::unexpected(error::not_enough_memory);
			}

			T* const ptr = ::new (storage.storage) T(vsm_forward(args)...);
			m_storage_ptr = ptr;
			return ptr;
		}
		else
		{
			return ::new (m_storage) T(vsm_forward(args)...);
		}
	}

	template<vsm::non_cvref T>
	[[nodiscard]] T* get()
	{
		if constexpr (requires_dynamic_storage<T>)
		{
			return static_cast<T*>(m_storage_ptr);
		}
		else
		{
			return static_cast<T*>(static_cast<void*>(m_storage));
		}
	}

	template<vsm::non_cvref T>
	[[nodiscard]] T const* get() const
	{
		if constexpr (requires_dynamic_storage<T>)
		{
			return static_cast<T const*>(m_storage_ptr);
		}
		else
		{
			return static_cast<T const*>(static_cast<void const*>(m_storage));
		}
	}

	template<vsm::non_cvref T>
	void destroy()
	{
		if constexpr (requires_dynamic_storage<T>)
		{
			std::destroy_at(static_cast<T*>(m_storage_ptr));

			allio_release_storage(
				m_storage_ptr,
				sizeof(T),
				alignof(T),
				allio_allocation_strategy_generic);
		}
		else
		{
			std::destroy_at(static_cast<T*>(static_cast<void*>(m_storage)));
		}
	}
};


template<typename BaseObject, typename... Multiplexers>
struct any_object_t;

template<typename AnyObject, operation_c Operation>
struct any_object_detached_functions_1
{
	template<typename T>
	using const_t = handle_const_t<Operation, T>;

	using result_type = io_result_t<basic_detached_handle<object_t>, Operation>;

	vsm::result<result_type>(* p_blocking_io)(
		const_t<native_handle<AnyObject>>& h,
		io_parameters_t<AnyObject, Operation> const& a);

	template<std::same_as<Operation>>
	vsm::result<result_type> blocking_io(
		const_t<native_handle<AnyObject>>& h,
		io_parameters_t<AnyObject, Operation> const& a) const
	{
		return p_blocking_io(h, a);
	}
};

template<typename AnyObject, multiplexer Multiplexer>
struct any_object_detached_functions_2
{
	vsm::result<void>(* p_attach_handle)(
		Multiplexer& m,
		native_handle<AnyObject> const& h,
		async_connector<Multiplexer, AnyObject>& c);

	template<std::same_as<Multiplexer>>
	vsm::result<void> attach_handle(
		multiplexer_handle<Multiplexer> auto&& m,
		native_handle<AnyObject> const& h,
		async_connector_t<Multiplexer, AnyObject>& c) const
	{
		return p_attach_handle(m, h, c);
	}
};

template<typename AnyObject, typename Operations, typename... Multiplexers>
struct any_object_detached_functions;

template<typename AnyObject, operation_c... Operations, typename... Multiplexers>
struct any_object_detached_functions<AnyObject, type_list<Operations...>, Multiplexers...>
	: any_object_detached_functions_1<AnyObject, Operations>...
	, any_object_detached_functions_2<AnyObject, Multiplexers>...
{
	using any_object_detached_functions_1<AnyObject, Operations>::blocking_io...;
	using any_object_detached_functions_2<AnyObject, Multiplexers>::attach_handle...;

	template<typename Object>
	static any_object_detached_functions const instance;
};

template<typename AnyObject, operation_c Operation, multiplexer Multiplexer>
struct any_object_attached_functions_1
{
	template<typename T>
	using const_t = handle_const_t<Operation, T>;

	using result_type = io_result_t<basic_attached_handle<object_t, Multiplexer>, Operation>;

	vsm::result<result_type>(* p_submit)(
		Multiplexer& m,
		const_t<native_handle<AnyObject>>& h,
		const_t<async_connector<Multiplexer, AnyObject>>& c,
		async_operation<Multiplexer, AnyObject, Operation>& s,
		io_parameters_t<AnyObject, Operation> const& a,
		io_handler<Multiplexer>& handler);

	vsm::result<result_type>(*p_notify)(
		Multiplexer& m,
		const_t<native_handle<AnyObject>>& h,
		const_t<async_connector<Multiplexer, AnyObject>>& c,
		async_operation<Multiplexer, AnyObject, Operation>& s,
		io_parameters_t<AnyObject, Operation> const& a,
		io_handler<Multiplexer>& handler,
		typename Multiplexer::io_status_type&& status);

	void(*p_cancel)(
		Multiplexer& m,
		native_handle<AnyObject> const& h,
		async_connector<Multiplexer, AnyObject> const& c,
		async_operation<Multiplexer, AnyObject, Operation>& s);

	template<std::same_as<Operation>>
	vsm::result<result_type> submit(
		multiplexer_handle_for<Multiplexer> auto&& m,
		const_t<native_handle<AnyObject>>& h,
		const_t<async_connector_t<Multiplexer, AnyObject>>& c,
		async_operation_t<Multiplexer, AnyObject, Operation>& s,
		io_parameters_t<AnyObject, Operation> const& a,
		io_handler<Multiplexer>& handler) const
	{
		return p_submit(m, h, c, s, a, handler);
	}

	template<std::same_as<Operation>>
	vsm::result<result_type> notify(
		multiplexer_handle_for<Multiplexer> auto&& m,
		const_t<native_handle<AnyObject>>& h,
		const_t<async_connector_t<Multiplexer, AnyObject>>& c,
		async_operation_t<Multiplexer, AnyObject, Operation>& s,
		io_parameters_t<AnyObject, Operation> const& a,
		io_handler<Multiplexer>& handler,
		typename Multiplexer::io_status_type&& status) const
	{
		return p_notify(m, h, c, s, a, handler, vsm_move(status));
	}

	template<std::same_as<Operation>>
	void cancel(
		multiplexer_handle_for<Multiplexer> auto&& m,
		native_handle<AnyObject> const& h,
		async_connector_t<Multiplexer, AnyObject> const& c,
		async_operation_t<Multiplexer, AnyObject, Operation>& s) const
	{
		p_cancel(m, h, c, s);
	}
};

template<typename AnyObject, typename Operations, multiplexer Multiplexer>
struct any_object_attached_functions;

template<typename AnyObject, operation_c... Operations, multiplexer Multiplexer>
struct any_object_attached_functions<AnyObject, type_list<Operations...>, Multiplexer>
	: any_object_attached_functions_1<AnyObject, Operations, Multiplexer>...
{
	using any_object_attached_functions_1<AnyObject, Operations, Multiplexer>::submit...;
	using any_object_attached_functions_1<AnyObject, Operations, Multiplexer>::notify...;
	using any_object_attached_functions_1<AnyObject, Operations, Multiplexer>::cancel...;

	vsm::result<void>(* p_detach_handle)(
		Multiplexer& m,
		native_handle<AnyObject> const& h,
		async_connector<Multiplexer, AnyObject>& c);

	vsm::result<void> detach_handle(
		multiplexer_handle<Multiplexer> auto&& m,
		native_handle<AnyObject> const& h,
		async_connector_t<Multiplexer, AnyObject>& c) const
	{
		return p_detach_handle(m, h, c);
	}

	template<typename Object>
	static any_object_attached_functions const instance;
};

template<typename BaseObject, typename... Multiplexers>
struct any_object_t : object_t
{
	using base_type = object_t;

	using operations = typename BaseObject::operations;

	template<operation_c Operation>
	using params_type = io_parameters_t<BaseObject, Operation>;

	template<operation_c Operation>
	static vsm::result<io_result_t<basic_detached_handle<any_object_t>, Operation>> blocking_io(
		handle_const_t<Operation, native_handle<any_object_t>>& h,
		io_parameters_t<any_object_t, Operation> const& a)
	{
		return h.functions->template blocking_io<Operation>(h, a);
	}

	template<typename Handle, typename Traits>
	using facade = typename base_type::template facade<Handle, Traits>;
};

template<typename BaseObject, typename... Multiplexers>
struct native_handle<any_object_t<BaseObject, Multiplexers...>> : native_handle<BaseObject>
{
	using functions_type = any_object_detached_functions<
		any_object_t<BaseObject, Multiplexers...>,
		typename BaseObject::operations,
		Multiplexers...>;

	functions_type const* functions;

	// TODO: Smarter control of the local storage size.
	any_object_storage<4> storage;

	explicit native_handle(functions_type const* const functions)
		: functions(functions)
	{
		native_handle<BaseObject>::flags = handle_flags::none;
	}
};

template<typename Multiplexer, typename BaseObject, typename... Multiplexers>
struct async_connector<Multiplexer, any_object_t<BaseObject, Multiplexers...>>
{
	using functions_type = any_object_attached_functions<
		any_object_t<BaseObject, Multiplexers...>,
		typename BaseObject::operations,
		Multiplexer>;

	functions_type const* functions;

	// TODO: Smarter control of the local storage size.
	any_object_storage<4> storage;

	explicit async_connector(functions_type const* const functions)
		: functions(functions)
	{
	}
};

template<typename Multiplexer, typename BaseObject, typename... Multiplexers, typename Operation>
struct async_operation<Multiplexer, any_object_t<BaseObject, Multiplexers...>, Operation>
{
	using M = Multiplexer;
	using T = any_object_t<BaseObject, Multiplexers...>;
	using H = handle_const_t<Operation, native_handle<T>>;
	using C = handle_const_t<Operation, async_connector_t<M, T>>;
	using S = async_operation_t<M, T, Operation>;
	using A = io_parameters_t<T, Operation>;
	using R = io_result_t<basic_attached_handle<T, M>, Operation>;

	// TODO: Smarter control of the local storage size.
	any_object_storage<8> storage;

	static io_result<R> submit(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& args,
		io_handler<M>& handler)
	{
		return c.functions->template submit<Operation>(
			m,
			h,
			c,
			s,
			args,
			handler);
	}

	static io_result<R> notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& args,
		io_handler<M>& handler,
		typename M::io_status_type status)
	{
		return c.functions->template notify<Operation>(
			m,
			h,
			c,
			s,
			args,
			handler,
			vsm_move(status));
	}

	static void cancel(M& m, H& h, C& c, S& s)
	{
		c.functions->template cancel<Operation>(
			m,
			h,
			c,
			s);
	}
};


template<typename AnyObject, typename Object, typename Operation>
vsm::result<io_result_t<basic_detached_handle<Object>, Operation>> any_blocking_io(
	handle_const_t<Operation, native_handle<AnyObject>>& any_h,
	io_parameters_t<AnyObject, Operation> const& a)
{
	using h_type = native_handle<Object>;

	auto const h = any_h.storage.template get<h_type>();
	auto r = blocking_io<Operation>(*h, a);

	if constexpr (consumer<Operation>)
	{
		if (!h->flags[object_t::flags::not_null])
		{
			any_h.storage.template destroy<h_type>();
			any_h.flags = handle_flags::none;
		}
	}

	return r;
}

template<typename AnyObject, typename Multiplexer, typename Object>
vsm::result<void> any_attach_handle(
	Multiplexer& m,
	native_handle<AnyObject> const& any_h,
	async_connector<Multiplexer, AnyObject>& any_c)
{
	using h_type = native_handle<Object>;
	using c_type = async_connector<Multiplexer, Object>;

	auto const h = any_h.storage.template get<h_type>();
	vsm_try(c, any_c.storage.template construct<c_type>());

	auto r = detail::attach_handle(m, *h, *c);

	if (!r)
	{
		any_c.storage.template destroy<c_type>();
	}

	return r;
}

template<typename AnyObject, typename Multiplexer, typename Object>
vsm::result<void> any_detach_handle(
	Multiplexer& m,
	native_handle<AnyObject> const& any_h,
	async_connector<Multiplexer, AnyObject>& any_c)
{
	using h_type = native_handle<Object>;
	using c_type = async_connector<Multiplexer, Object>;

	auto const h = any_h.storage.template get<h_type>();
	auto const c = any_c.storage.template get<c_type>();

	auto r = detail::detach_handle(m, *h, *c);

	if (r)
	{
		any_c.storage.template destroy<c_type>();
	}

	return r;
}

template<typename AnyObject, typename Multiplexer, typename Object, typename Operation>
io_result<io_result_t<basic_attached_handle<Object, Multiplexer>, Operation>> any_submit(
	Multiplexer& m,
	handle_const_t<Operation, native_handle<AnyObject>>& any_h,
	handle_const_t<Operation, async_connector<Multiplexer, AnyObject>>& any_c,
	async_operation<Multiplexer, AnyObject, Operation>& any_s,
	io_parameters_t<AnyObject, Operation> const& a,
	io_handler<Multiplexer>& handler)
{
	using h_type = native_handle<Object>;
	using c_type = async_connector<Multiplexer, Object>;
	using s_type = async_operation<Multiplexer, Object, Operation>;

	auto const h = any_h.storage.template get<h_type>();
	auto const c = any_c.storage.template get<c_type>();
	vsm_try(s, any_s.storage.template construct<s_type>());

	auto r = detail::submit_io(m, *h, *c, *s, a, handler);

	if (detail::get_io_notify_status(r) != io_notify_status::submitted)
	{
		any_s.storage.template destroy<s_type>();

		if constexpr (consumer<Operation>)
		{
			if (!h->flags[object_t::flags::not_null])
			{
				any_c.storage.template destroy<c_type>();
				any_h.storage.template destroy<h_type>();
				any_h.flags = handle_flags::none;
			}
		}
	}

	return r;
}

template<typename AnyObject, typename Multiplexer, typename Object, typename Operation>
io_result<io_result_t<basic_attached_handle<Object, Multiplexer>, Operation>> any_notify(
	Multiplexer& m,
	handle_const_t<Operation, native_handle<AnyObject>>& any_h,
	handle_const_t<Operation, async_connector<Multiplexer, AnyObject>>& any_c,
	async_operation<Multiplexer, AnyObject, Operation>& any_s,
	io_parameters_t<AnyObject, Operation> const& a,
	io_handler<Multiplexer>& handler,
	typename Multiplexer::io_status_type status)
{
	using h_type = native_handle<Object>;
	using c_type = async_connector<Multiplexer, Object>;
	using s_type = async_operation<Multiplexer, Object, Operation>;

	auto const h = any_h.storage.template get<h_type>();
	auto const c = any_c.storage.template get<c_type>();
	auto const s = any_s.storage.template get<s_type>();

	auto r = detail::notify_io(m, *h, *c, *s, a, handler, vsm_move(status));

	if (detail::get_io_notify_status(r) != io_notify_status::submitted)
	{
		any_s.storage.template destroy<s_type>();

		if constexpr (consumer<Operation>)
		{
			if (!h->flags[object_t::flags::not_null])
			{
				any_c.storage.template destroy<c_type>();
				any_h.storage.template destroy<h_type>();
				any_h.flags = handle_flags::none;
			}
		}
	}

	return r;
}

template<typename AnyObject, typename Multiplexer, typename Object, typename Operation>
void any_cancel(
	Multiplexer& m,
	native_handle<AnyObject> const& any_h,
	async_connector<Multiplexer, AnyObject> const& any_c,
	async_operation<Multiplexer, AnyObject, Operation>& any_s)
{
	using h_type = native_handle<Object>;
	using c_type = async_connector<Multiplexer, Object>;
	using s_type = async_operation<Multiplexer, Object, Operation>;

	auto const h = any_h.storage.template get<h_type>();
	auto const c = any_c.storage.template get<c_type>();
	auto const s = any_s.storage.template get<s_type>();

	detail::cancel_io(m, *h, *c, *s);
}

// TODO: Intellisense was having issues here. Figure out if there's a workaround.
#ifndef __INTELLISENSE__
template<typename AnyObject, operation_c... Operations, typename... Multiplexers>
template<typename Object>
any_object_detached_functions<AnyObject, type_list<Operations...>, Multiplexers...> const
any_object_detached_functions<AnyObject, type_list<Operations...>, Multiplexers...>::instance =
{
	{
		any_blocking_io<AnyObject, Object, Operations>
	}...,

	{
		any_attach_handle<AnyObject, Multiplexers, Object>
	}...,
};

template<typename AnyObject, operation_c... Operations, multiplexer Multiplexer>
template<typename Object>
any_object_attached_functions<AnyObject, type_list<Operations...>, Multiplexer> const
any_object_attached_functions<AnyObject, type_list<Operations...>, Multiplexer>::instance =
{
	{
		any_submit<AnyObject, Multiplexer, Object, Operations>,
		any_notify<AnyObject, Multiplexer, Object, Operations>,
		any_cancel<AnyObject, Multiplexer, Object, Operations>,
	}...,

	any_detach_handle<AnyObject, Multiplexer, Object>,
};
#endif


template<typename AnyObject, typename Object>
[[nodiscard]] vsm::result<basic_detached_handle<AnyObject>> make_any()
{
	using h_type = native_handle<Object>;
	using h_functions_type = typename native_handle<AnyObject>::functions_type;

	auto const h_functions = &h_functions_type::template instance<Object>;

	return vsm::result<basic_detached_handle<AnyObject>>(
		vsm::result_value,
		adopt_handle,
		h_type(h_functions));
}

template<typename AnyObject, typename MultiplexerHandle, typename Object>
[[nodiscard]] vsm::result<basic_attached_handle<AnyObject, MultiplexerHandle>> make_any(
	vsm::convertible_to<MultiplexerHandle> auto&& multiplexer_handle)
{
	using multiplexer_type = typename MultiplexerHandle::multiplexer_type;

	using h_type = native_handle<Object>;
	using h_functions_type = typename native_handle<AnyObject>::functions_type;

	using c_type = async_connector<multiplexer_type, Object>;
	using c_functions_type = typename async_connector<multiplexer_type, AnyObject>::functions_type;

	auto const h_functions = &h_functions_type::template instance<Object>;
	auto const c_functions = &c_functions_type::template instance<Object>;

	return vsm::result<basic_attached_handle<AnyObject, MultiplexerHandle>>(
		vsm::result_value,
		adopt_handle,
		vsm_forward(multiplexer_handle),
		h_type(h_functions),
		c_type(c_functions));
}


template<typename AnyObject, typename Object>
[[nodiscard]] vsm::result<basic_detached_handle<AnyObject>> make_any(
	basic_detached_handle<Object>&& handle)
{
	using h_type = native_handle<Object>;
	using h_functions_type = typename native_handle<AnyObject>::functions_type;

	auto const h_functions = &h_functions_type::template instance<Object>;

	native_handle<AnyObject> new_h(h_functions);
	vsm_try(h_storage, new_h.storage.template allocate<h_type>());
	new_h.storage.template construct_at<h_type>(h_storage, handle.release());

	return vsm::result<basic_detached_handle<AnyObject>>(
		vsm::result_value,
		adopt_handle,
		vsm_move(new_h));
}

template<typename AnyObject, typename MultiplexerHandle, typename Object>
[[nodiscard]] vsm::result<basic_attached_handle<AnyObject, MultiplexerHandle>> make_any(
	basic_attached_handle<Object, MultiplexerHandle>&& handle)
{
	using multiplexer_type = typename MultiplexerHandle::multiplexer_type;

	using h_type = native_handle<Object>;
	using h_functions_type = typename native_handle<AnyObject>::functions_type;

	using c_type = async_connector<multiplexer_type, Object>;
	using c_functions_type = typename async_connector<multiplexer_type, AnyObject>::functions_type;

	auto const h_functions = &h_functions_type::template instance<Object>;
	auto const c_functions = &c_functions_type::template instance<Object>;

	native_handle<AnyObject> new_h(h_functions);
	async_connector<multiplexer_type, AnyObject> new_c(c_functions);

	vsm_try(h_storage, new_h.storage.template allocate<h_type>());
	vsm_try(c_storage, new_c.storage.template allocate<c_type>());

	auto [h, c] = handle.release();

	new_h.storage.template construct_at<h_type>(h_storage, vsm_move(h));
	new_c.storage.template construct_at<c_type>(c_storage, vsm_move(c));

	return vsm::result<basic_attached_handle<AnyObject, MultiplexerHandle>>(
		vsm::result_value,
		adopt_handle,
		vsm_move(handle).multiplexer(),
		vsm_move(new_h),
		vsm_move(new_c));
}

} // namespace allio::detail
