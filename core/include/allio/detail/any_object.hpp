#pragma once

#include <allio/detail/handle.hpp>
#include <allio/detail/type_list.hpp>

namespace allio::detail {

template<typename ModelObject, typename... Multiplexers>
struct any_object_t;

template<typename Object, operation_c Operation>
struct any_object_detached_functions_1
{
	template<typename T>
	using const_t = handle_const_t<Operation, void>;

	using result_type = io_result_t<basic_detached_handle<object_t>, Operation>;

	vsm::result<result_type>(* blocking)(
		const_t<void>* h,
		io_parameters_t<void, Operation> const& a);

	vsm::result<result_type> operator()(
		blocking_io_t<Operation>,
		const_t<native_handle<Object>> const& h,
		io_parameters_t<void, Operation> const& a) const
	{
		return blocking(h.storage, a);
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
	using const_t = handle_const_t<Operation, void>;

	using result_type = io_result_t<basic_attached_handle<object_t, Multiplexer>, Operation>;

	vsm::result<result_type>(* submit)(
		Multiplexer& m,
		const_t<void>* h,
		const_t<void>* c,
		async_operation_t<Multiplexer, Object, Operation>& s,
		io_parameters_t<void, Operation> const& a,
		io_handler<Multiplexer>& handler);

	vsm::result<result_type>(* notify)(
		Multiplexer& m,
		const_t<void>* h,
		const_t<void>* c,
		async_operation_t<Multiplexer, Object, Operation>& s,
		io_parameters_t<void, Operation> const& a,
		typename Multiplexer::io_status_type&& status);

	vsm::result<void>(* cancel)(
		Multiplexer& m,
		void const* h,
		void const* c,
		async_operation_t<Multiplexer, Object, Operation>& s);

	vsm::result<result_type> operator()(
		submit_io_t<Operation>
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
			s,
			a,
			handler);
	}

	vsm::result<result_type> operator()(
		notify_io_t<Operation>
		multiplexer_handle_for<Multiplexer> auto&& m,
		const_t<native_handle<Object>>& h,
		const_t<async_connector_t<Multiplexer, Object>>& c,
		async_operation_t<Multiplexer, Object, Operation>& s,
		io_parameters_t<Object, Operation> const& a,
		typename Multiplexer::io_status_type&& status) const
	{
		return notify(
			m,
			h.storage,
			c.storage,
			s,
			a,
			vsm_move(status));
	}

	vsm::result<result_type> operator()(
		cancel_io_t<Operation>
		multiplexer_handle_for<Multiplexer> auto&& m,
		native_handle<Object> const& h,
		async_connector_t<Multiplexer, Object> const& c,
		async_operation_t<Multiplexer, Object, Operation>& s) const
	{
		return cancel(
			m,
			h.storage,
			c.storage,
			s);
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

template<typename ModelObject, typename... Multiplexers>
struct any_object_t : object_t
{
	using base_type = object_t;

	using operations = typename ModelObject::operations;

	template<typename Handle, typename Traits>
	using facade = typename ModelObject::template facade<Handle, Traits>;
};

template<typename ModelObject, typename... Multiplexers>
struct native_handle<any_object_t<ModelObject, Multiplexers...>> : native_handle<object_t>
{
	any_object_detached_functions<
		any_object_t<ModelObject, Multiplexers...>,
		typename ModelObject::operations,
		Multiplexers...
	> const* functions;

	//TODO: Smarter control of the local storage size.
	alignas(std::max_align_t) unsigned char storage[4 * sizeof(void*)];
};

template<typename Multiplexer, typename ModelObject, typename... Multiplexers>
struct async_connector<Multiplexer, any_object_t<ModelObject, Multiplexers...>>
{
	any_object_attached_functions<
		any_object_t<ModelObject, Multiplexers...>,
		typename ModelObject::operations,
		Multiplexer
	> const* functions;

	//TODO: Smarter control of the local storage size.
	alignas(std::max_align_t) unsigned char storage[4 * sizeof(void*)];
};

} // namespace allio::detail
