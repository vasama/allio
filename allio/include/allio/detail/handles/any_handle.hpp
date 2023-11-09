#pragma once

#include <allio/detail/handle.hpp>
#include <allio/detail/type_erasure.hpp>

#include <vsm/concepts.hpp>

namespace allio::detail {

struct any_handle_connector_tag;

template<typename BaseHandleTag, typename... Multiplexers>
struct any_handle_t : BaseHandleTag
{
	using base_type = BaseHandleTag;

	struct native_type;

	template<typename... Os>
	using virtual_table_template = virtual_table<
		vsm::result<io_result_t<Os>>(blocking_io_t, virtual_object<handle_cv<Os, native_type>>, io_parameters_t<Os> const& args)...
		vsm::result<void>(attach_handle_t, Multiplexer&, virtual_object<BaseHandleTag const>, virtual_object<any_handle_connector_tag>)...
		vsm::result<void>(detach_handle_t, Multiplexer&, virtual_object<BaseHandleTag const>, virtual_object<any_handle_connector_tag>)...
	>;

	using virtual_table_type = any_handle_virtual_table<
		BaseHandleTag,
		typename BaseHandleTag::operations>;

	struct native_type : base_type::native_type
	{
		virtual_table_type const* virtual_table;

		template<typename Tag, typename... Args>
		friend auto tag_invoke(Tag const tag, Args&&... args)
			-> decltype((*h.virtual_table)(tag, vsm_forward(args)...))
		{
			return (*h.virtual_table)(tag, vsm_forward(args)...);
		}
	};
};

template<typename H, typename M, typename... Os>
using any_handle_connector_virtual_table = virtual_table<
>;

template<typename Multiplexer, typename BaseHandleTag>
struct connector_impl<Multiplexer, any_handle_t<BaseHandleTag>>
{
	using M = Multiplexer;
	using H = any_handle_t<BaseHandleTag>;
	using N = H::native_type;
	using C = connector_t<M, H>;

	template<typename... Os>
	using virtual_table_template = virtual_table<
		io_result2<io_result_t<Os, M>>(submit_io_t, virtual_object<handle_cv<Os, N>>, virtual_object<C const>, operation_t<M, H, Os>&)...,
		io_result2<io_result_t<Os, M>>(notify_io_t, virtual_object<handle_cv<Os, N>>, virtual_object<C const>, operation_t<M, H, Os>&, io_status)...,
		void                          (cancel_io_t, virtual_object<handle_cv<Os, N>>, virtual_object<C const>, operation_t<M, H, Os>&)...
	>;

	using virtual_table_type = typename BaseHandleTag::operations::template apply<virtual_table_template>;

	virtual_table_type const* virtual_table;
};

template<typename Multiplexer, typename BaseHandleTag, typename O>
struct operation_impl<Multiplexer, any_handle_t<BaseHandleTag>, O>
{
	using M = Multiplexer;
	using H = any_handle_t<BaseHandleTag>;
	using N = typename H::native_type;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	static io_result2<io_result_t<O, M>> submit_io(M& m, handle_cv<N, M>& h, C const& c, S& s)
	{
		return (*c.virtual_table)(submit_io_t, h, c, s);
	}

	static io_result2<io_result_t<O, M>> notify_io(M& m, handle_cv<N, M>& h, C const& c, S& s, io_status const status)
	{
		return (*c.virtual_table)(notify_io_t, h, c, s, status);
	}

	static void cancel_io(M& m, handle_cv<N, M>& h, C const& c, S& s)
	{
		(*c.virtual_table)(cancel_io_t, h, c, s);
	}
};

} // namespace allio::detail
