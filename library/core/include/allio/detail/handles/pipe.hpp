#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/handles/platform_object.hpp>

#include <vsm/lazy.hpp>

namespace allio::detail {

template<typename PipeHandle>
struct basic_pipe_pair
{
	PipeHandle read_pipe;
	PipeHandle write_pipe;
};

struct pipe_t : platform_object_t
{
	using base_type = platform_object_t;

	using stream_read_t = byte_io::stream_read_t;
	using stream_write_t = byte_io::stream_write_t;

	using operations = type_list_append
	<
		base_type::operations
		, stream_read_t
		, stream_write_t
	>;

	static byte_io_limits get_byte_io_limits(native_handle<pipe_t> const& h);

	static vsm::result<size_t> stream_read(
		native_handle<pipe_t> const& h,
		io_parameters_t<pipe_t, stream_read_t> const& a);

	static vsm::result<size_t> stream_write(
		native_handle<pipe_t> const& h,
		io_parameters_t<pipe_t, stream_write_t> const& a);


	using is_serializable = pipe_t;

	static vsm::result<void> serialize(
		native_handle<pipe_t>& h,
		serialization_context& serializer);

	static bool verify_handle(native_handle<pipe_t> const& h);


	template<typename Handle, typename Traits>
	struct facade
		: base_type::facade<Handle, Traits>
		, byte_io::stream_facade<Handle, Traits>
	{
	};
};

struct pipe_pair_t : object_t
{
	using base_type = object_t;

	struct create_pair_t
	{
		using operation_concept = producer_t;

		struct params_type
		{
			io_flags_t read_pipe;
			io_flags_t write_pipe;

			void set_argument(auto const& value)
			{
				detail::set_argument(read_pipe, value);
				detail::set_argument(write_pipe, value);
			}
		};

		using result_type = void;
		using runtime_concept = bounded_runtime_t;

		template<object Object>
		static vsm::result<void> blocking_io(
			native_handle<Object>& h,
			io_parameters_t<Object, create_pair_t> const& a)
			requires requires { Object::create_pair(h, a); }
		{
			return Object::create_pair(h, a);
		}
	};

	using operations = type_list_append
	<
		base_type::operations
		, create_pair_t
	>;

	static vsm::result<void> create_pair(
		native_handle<pipe_pair_t>& h,
		io_parameters_t<pipe_pair_t, create_pair_t> const& a);

	static vsm::result<void> close(
		native_handle<pipe_pair_t>& h,
		io_parameters_t<pipe_pair_t, close_t> const& a);
};

template<>
struct native_handle<pipe_pair_t> : native_handle<pipe_pair_t::base_type>
{
	native_handle<pipe_t> r_h;
	native_handle<pipe_t> w_h;
};

#if 0
template<typename Traits>
[[nodiscard]] vsm::result<basic_pipe_pair<typename Traits::template handle<pipe_t>>> _create_pipe(
	io_parameters_t<pipe_pair_t, pipe_pair_t::create_pair_t> const& a)
{
	using handle_type = typename Traits::template handle<pipe_t>;
	vsm::result<basic_pipe_pair<handle_type>> r(vsm::result_value);

	native_handle<pipe_pair_t> h = {};
	vsm_try_void(blocking_io<pipe_pair_t::create_pair_t>(h, a));

	basic_detached_handle<pipe_t> r_h(adopt_handle, h.r_h);
	basic_detached_handle<pipe_t> w_h(adopt_handle, h.w_h);

	vsm_try_assign(r->read_pipe, rebind_handle<handle_type>(vsm_move(r_h)));
	vsm_try_assign(r->write_pipe, rebind_handle<handle_type>(vsm_move(w_h)));

	return r;
}
#endif

template<
	detached_handle_for<pipe_pair_t> PairHandle,
	std::same_as<typename PairHandle::template rebind_object<pipe_t>> Handle>
struct rebind_traits<PairHandle, basic_pipe_pair<Handle>>
{
	static vsm::result<basic_pipe_pair<Handle>> rebind(vsm::any_cvref_of<PairHandle> auto&& pair_h)
	{
		auto h = pair_h.release();
		return vsm::result<basic_pipe_pair<Handle>>(
			vsm::result_value,
			vsm_lazy(Handle(adopt_handle, h.r_h)),
			vsm_lazy(Handle(adopt_handle, h.w_h)));
	}
};

template<
	attached_handle_for<pipe_pair_t> PairHandle,
	std::same_as<typename PairHandle::template rebind_object<pipe_t>> Handle>
struct rebind_traits<PairHandle, basic_pipe_pair<Handle>>
{
	static vsm::result<basic_pipe_pair<Handle>> rebind(vsm::any_cvref_of<PairHandle> auto&& pair_h)
	{
		auto h = pair_h.release();
		auto const& m = pair_h.multiplexer();
		return vsm::result<basic_pipe_pair<Handle>>(
			vsm::result_value,
			vsm_lazy(Handle(adopt_handle, m, vsm_move(h.handle.r_h), vsm_move(h.connector.r_c))),
			vsm_lazy(Handle(adopt_handle, m, vsm_move(h.handle.w_h), vsm_move(h.connector.w_c))));
	}
};

template<typename Traits>
[[nodiscard]] auto create_pipe(auto&&... args)
{
	auto a = io_parameters_t<pipe_pair_t, pipe_pair_t::create_pair_t>{};
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::transform(
		Traits::template produce<pipe_pair_t, pipe_pair_t::create_pair_t>(a),
		[]<typename PipePairHandle>(PipePairHandle&& h)
		{
			// Simply splitting the two handles apart into a pair always succeeeds:
			using handle_type = typename PipePairHandle::template rebind_object<pipe_t>;
			return vsm::assume_success(rebind_handle<basic_pipe_pair<handle_type>>(vsm_forward(h)));
		});

#if 0
	auto r = _create_pipe<Traits>(a);

	if constexpr (Traits::has_transform_result)
	{
		return Traits::transform_result(vsm_move(r));
	}
	else
	{
		return r;
	}
#endif
}

template<typename Multiplexer>
struct async_create_pipe_pair : Multiplexer::operation_type
{
	using M = Multiplexer;
	using H = native_handle<pipe_pair_t>;
	using C = async_connector<M, pipe_pair_t>;
	using S = async_operation<M, pipe_pair_t, pipe_pair_t::create_pair_t>;
	using A = io_parameters_t<pipe_pair_t, pipe_pair_t::create_pair_t>;

	static io_result<void> submit(M& m, H& h, C& c, S&, A const& a_ref, io_handler<M>&)
	{
		A a = a_ref;

		vsm_try_void(blocking_io<pipe_pair_t::create_pair_t>(h, a));

		if (auto const r = attach_handle(m, h.r_h, c.r_c); !r)
		{
			unrecoverable(blocking_io<close_t>(h, no_parameters_t()));
			return vsm::unexpected(r.error());
		}

		if (auto const r = attach_handle(m, h.w_h, c.w_c); !r)
		{
			unrecoverable(detach_handle(m, h.r_h, c.r_c));
			unrecoverable(blocking_io<close_t>(h, no_parameters_t()));
			return vsm::unexpected(r.error());
		}

		return {};
	}

	static io_result<void> notify(
		M& m,
		H& h,
		C& c,
		S&,
		A const& a,
		io_handler<M>&,
		typename M::io_status_type&&)
	{
		vsm_unreachable();
	}

	static void cancel(
		M&,
		H const&,
		C const&,
		S&)
	{
		vsm_unreachable();
	}
};

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/pipe.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/pipe.hpp>
#endif

template<typename Multiplexer>
struct allio::detail::async_connector<Multiplexer, allio::detail::pipe_pair_t>
{
	async_connector<Multiplexer, pipe_t> r_c;
	async_connector<Multiplexer, pipe_t> w_c;

	static vsm::result<void> attach(
		Multiplexer& m,
		native_handle<pipe_pair_t>& h,
		async_connector<Multiplexer, pipe_pair_t>& c) = delete;

	static vsm::result<void> detach(
		Multiplexer& m,
		native_handle<pipe_pair_t>& h,
		async_connector<Multiplexer, pipe_pair_t>& c) = delete;
};

template<typename Multiplexer>
struct allio::detail::async_operation<
	Multiplexer,
	allio::detail::pipe_pair_t,
	allio::detail::pipe_pair_t::create_pair_t>
	: async_create_pipe_pair<Multiplexer>
{
};
