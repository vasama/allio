#pragma once

#include <allio/detail/handle.hpp>
#include <allio/detail/type_list.hpp>

namespace allio::detail {

template<typename Object, typename Operation>
vsm::result<io_result_t<basic_detached_handle<Object>, Operation>> any_blocking_io(
	handle_const_t<Operation, void>* h_storage,
	io_parameters_t<Object, Operation> const& a)
{
	auto& h = *static_cast<handle_const_t<Operation, native_handle<Object>>*>(h);

	auto r = blocking_io<Operation>(h, a);

	if constexpr (std::is)

	return r;
}


template<typename BaseObject, typename... Multiplexers>
struct any_object_t;

template<typename Object, operation_c Operation>
struct any_object_detached_functions_1
{
	template<typename T>
	using const_t = handle_const_t<Operation, T>;

	using result_type = io_result_t<basic_detached_handle<object_t>, Operation>;

	vsm::result<result_type>(* blocking_io)(
		const_t<void>* h,
		io_parameters_t<void, Operation> const& a);

	vsm::result<result_type> operator()(
		blocking_io_t<Operation>,
		const_t<native_handle<Object>> const& h,
		io_parameters_t<void, Operation> const& a) const
	{
		return blocking_io(h.storage, a);
	}
};

template<typename Object, multiplexer Multiplexer>
struct any_object_detached_functions_2
{
	vsm::result<void>(* attach_handle)(
		Multiplexer& m,
		void const* h,
		void* c);

	vsm::result<void> operator()(
		attach_handle_t,
		multiplexer_handle<Multiplexer> auto&& m,
		native_handle<Object> const& h,
		async_connector_t<Multiplexer, Object>& c) const
	{
		return attach_handle(
			m,
			h.storage,
			c.storage);
	}
};

template<typename Object, typename Operations, typename... Multiplexers>
struct any_object_detached_functions;

template<typename Object, operation_c... Operations, typename... Multiplexers>
struct any_object_detached_functions<Object, type_list<Operations...>, Multiplexers...>
	: any_object_detached_functions_1<Object, Operations>...
	, any_object_detached_functions_2<Object, Multiplexers>...
{
	using any_object_detached_functions_1<Object, Operations>::operator()...;
	using any_object_detached_functions_2<Object, Multiplexers>::operator()...;
};

template<typename Object, operation_c Operation, multiplexer Multiplexer>
struct any_object_attached_functions_1
{
	template<typename T>
	using const_t = handle_const_t<Operation, T>;

	using result_type = io_result_t<basic_attached_handle<object_t, Multiplexer>, Operation>;

	vsm::result<result_type>(* submit)(
		Multiplexer& m,
		const_t<void>* h,
		const_t<void>* c,
		void* s,
		io_parameters_t<void, Operation> const& a,
		io_handler<Multiplexer>& handler);

	vsm::result<result_type>(* notify)(
		Multiplexer& m,
		const_t<void>* h,
		const_t<void>* c,
		void* s,
		io_parameters_t<void, Operation> const& a,
		io_handler<Multiplexer>& handler,
		typename Multiplexer::io_status_type&& status);

	vsm::result<void>(* cancel)(
		Multiplexer& m,
		void const* h,
		void const* c,
		void* s);

	vsm::result<result_type> operator()(
		submit_io_t,
		multiplexer_handle_for<Multiplexer> auto&& m,
		const_t<native_handle<Object>>& h,
		const_t<async_connector_t<Multiplexer, Object>>& c,
		async_operation_t<Multiplexer, Object, Operation>& s,
		io_parameters_t<Object, Operation> const& a,
		io_handler<Multiplexer>& handler) const
	{
		return submit(
			m,
			h.storage,
			c.storage,
			s.storage,
			a,
			handler);
	}

	vsm::result<result_type> operator()(
		notify_io_t,
		multiplexer_handle_for<Multiplexer> auto&& m,
		const_t<native_handle<Object>>& h,
		const_t<async_connector_t<Multiplexer, Object>>& c,
		async_operation_t<Multiplexer, Object, Operation>& s,
		io_parameters_t<Object, Operation> const& a,
		io_handler<Multiplexer>& handler,
		typename Multiplexer::io_status_type&& status) const
	{
		return notify(
			m,
			h.storage,
			c.storage,
			s.storage,
			a,
			handler,
			vsm_move(status));
	}

	vsm::result<result_type> operator()(
		cancel_io_t,
		multiplexer_handle_for<Multiplexer> auto&& m,
		native_handle<Object> const& h,
		async_connector_t<Multiplexer, Object> const& c,
		async_operation_t<Multiplexer, Object, Operation>& s) const
	{
		return cancel(
			m,
			h.storage,
			c.storage,
			s.storage);
	}
};

template<typename Object, typename Operations, multiplexer Multiplexer>
struct any_object_attached_functions;

template<typename Object, operation_c... Operations, multiplexer Multiplexer>
struct any_object_attached_functions<Object, type_list<Operations...>, Multiplexer>
	: any_object_attached_functions_1<Object, Operations, Multiplexer>...
{
	using any_object_attached_functions_1<Object, Operations, Multiplexer>::operator()...;

	vsm::result<void>(* detach_handle)(
		Multiplexer& m,
		void const* h,
		void* c);

	vsm::result<void> operator()(
		detach_handle_t,
		multiplexer_handle<Multiplexer> auto&& m,
		native_handle<Object> const& h,
		async_connector_t<Multiplexer, Object>& c) const
	{
		return detach_handle(
			m,
			h.storage,
			c.storage);
	}
};

template<typename BaseObject, typename... Multiplexers>
struct any_object_t : BaseObject
{
	using base_type = BaseObject;
};

template<typename BaseObject, typename... Multiplexers>
struct native_handle<any_object_t<BaseObject, Multiplexers...>> : native_handle<BaseObject>
{
	any_object_detached_functions<
		any_object_t<BaseObject, Multiplexers...>,
		typename BaseObject::operations,
		Multiplexers...
	> const* functions;

	// TODO: Smarter control of the local storage size.
	alignas(std::max_align_t) unsigned char storage[4 * sizeof(void*)];
};

template<typename Multiplexer, typename BaseObject, typename... Multiplexers>
struct async_connector<Multiplexer, any_object_t<BaseObject, Multiplexers...>>
{
	any_object_attached_functions<
		any_object_t<BaseObject, Multiplexers...>,
		typename BaseObject::operations,
		Multiplexer
	> const* functions;

	// TODO: Smarter control of the local storage size.
	alignas(std::max_align_t) unsigned char storage[4 * sizeof(void*)];
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
	alignas(std::max_align_t) unsigned char storage[8 * sizeof(void*)];

	static io_result<R> submit(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& args,
		io_handler<M>& handler)
	{
		return (*c.functions)(
			submit_io,
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
		return (*c.functions)(
			notify_io,
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
		(*c.functions)(
			notify_io,
			m,
			h,
			c,
			s);
	}
};

} // namespace allio::detail
