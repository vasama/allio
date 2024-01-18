#pragma once

#include <allio/detail/exceptions.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

template<handle Handle, typename Traits>
class basic_facade;

template<detached_handle Handle, typename Traits>
class basic_facade<Handle, Traits>
	: public Handle
	, public Handle::object_type::template facade<basic_facade<Handle, Traits>, Traits>
{
	template<typename OtherHandle>
	using rebind = basic_facade<OtherHandle, Traits>;

public:
	template<object OtherObject>
	using rebind_object = basic_facade<typename Handle::template rebind_object<OtherObject>, Traits>;

	template<optional_multiplexer_handle_for<typename Handle::object_type> OtherMultiplexerHandle>
	using rebind_multiplexer = basic_facade<typename Handle::template rebind_multiplexer<OtherMultiplexerHandle>, Traits>;


	using Handle::Handle;

	explicit basic_facade(Handle&& handle)
		: Handle(vsm_move(handle))
	{
	}

	template<multiplexer_for<typename Handle::object_type> Multiplexer>
	[[nodiscard]] auto via(Multiplexer& multiplexer) &&
	{
		return _via<multiplexer_handle_t<Multiplexer>>(multiplexer);
	}

	template<typename MultiplexerHandle>
		requires multiplexer_handle_for<std::remove_cvref_t<MultiplexerHandle>, typename Handle::object_type>
	[[nodiscard]] auto via(MultiplexerHandle&& multiplexer) &&
	{
		return _via<std::remove_cvref_t<MultiplexerHandle>>(vsm_forward(multiplexer));
	}

	template<multiplexer_handle_for<typename Handle::object_type> MultiplexerHandle>
	[[nodiscard]] auto via(std::convertible_to<MultiplexerHandle> auto&& multiplexer) &&
	{
		return _via<MultiplexerHandle>(vsm_forward(multiplexer));
	}

private:
	template<multiplexer_handle_for<typename Handle::object_type> MultiplexerHandle>
	[[nodiscard]] auto _via(std::convertible_to<MultiplexerHandle> auto&& multiplexer)
	{
		using handle_type = typename Handle::template rebind_multiplexer<MultiplexerHandle>;

#if 0
		auto r = tag_invoke(
			rebind_handle_t<rebind<handle_type>>(),
			vsm_move(*this),
			vsm_forward(multiplexer));
#else
		auto r = rebind_handle<rebind<handle_type>>(
			vsm_move(*this),
			vsm_forward(multiplexer));
#endif

		if constexpr (Traits::has_transform_result)
		{
			return Traits::transform_result(vsm_move(r));
		}
		else
		{
			return r;
		}
	}

	friend vsm::result<basic_facade> tag_invoke(
		rebind_handle_t<basic_facade>,
		Handle&& h)
	{
		return vsm::result<basic_facade>(vsm::result_value, vsm_move(h));
	}

	friend vsm::result<Handle> tag_invoke(
		rebind_handle_t<Handle>,
		basic_facade&& h)
	{
		return vsm::result<Handle>(vsm::result_value, vsm_move(h));
	}

	friend vsm::result<basic_facade> tag_invoke(
		rebind_handle_t<basic_facade>,
		vsm::not_same_as<basic_facade> auto&& h,
		auto&&... args)
		requires requires { rebind_handle<Handle>(vsm_forward(h), vsm_forward(args)...); }
	{
		vsm_try(new_h, rebind_handle<Handle>(
			vsm_forward(h),
			vsm_forward(args)...));

		return vsm::result<basic_facade>(
			vsm::result_value,
			vsm_move(new_h));
	}

#if 0
	template<handle OtherHandle>
	friend vsm::result<basic_facade<OtherHandle, Traits>> tag_invoke(
		rebind_handle_t<basic_facade<OtherHandle, Traits>>,
		basic_facade&& h,
		auto&&... args)
	{
		vsm_try(new_h, rebind_handle<OtherHandle>(
			static_cast<Handle&&>(h),
			vsm_forward(args)...));

		return vsm::result<basic_facade<OtherHandle, Traits>>(
			vsm::result_value,
			vsm_move(new_h));
	}
#endif
};

template<attached_handle Handle, typename Traits>
class basic_facade<Handle, Traits>
	: public Handle
	, public Handle::object_type::template facade<basic_facade<Handle, Traits>, Traits>
{
	template<typename OtherHandle>
	using rebind = basic_facade<OtherHandle, Traits>;

public:
	template<object OtherObject>
	using rebind_object = basic_facade<typename Handle::template rebind_object<OtherObject>, Traits>;

	template<optional_multiplexer_handle_for<typename Handle::object_type> OtherMultiplexerHandle>
	using rebind_multiplexer = basic_facade<typename Handle::template rebind_multiplexer<OtherMultiplexerHandle>, Traits>;


	using Handle::Handle;

	explicit basic_facade(Handle&& handle)
		: Handle(vsm_move(handle))
	{
	}

	[[nodiscard]] auto detach() &&
	{
		using handle_type = typename Handle::template rebind_multiplexer<void>;
		auto r = rebind_handle<rebind<handle_type>>(vsm_move(*this));

		if constexpr (Traits::has_transform_result)
		{
			return Traits::transform_result(vsm_move(r));
		}
		else
		{
			return r;
		}
	}

private:
	friend vsm::result<basic_facade> tag_invoke(
		rebind_handle_t<basic_facade>,
		Handle&& h)
	{
		return vsm::result<basic_facade>(vsm::result_value, vsm_move(h));
	}

	friend vsm::result<Handle> tag_invoke(
		rebind_handle_t<Handle>,
		basic_facade&& h)
	{
		return vsm::result<Handle>(vsm::result_value, vsm_move(h));
	}

	friend vsm::result<basic_facade> tag_invoke(
		rebind_handle_t<basic_facade>,
		vsm::not_same_as<basic_facade> auto&& h,
		auto&&... args)
		requires requires { rebind_handle<Handle>(vsm_forward(h), vsm_forward(args)...); }
	{
		vsm_try(new_h, rebind_handle<Handle>(
			vsm_forward(h),
			vsm_forward(args)...));

		return vsm::result<basic_facade>(
			vsm::result_value,
			vsm_move(new_h));
	}

#if 0
	template<handle OtherHandle>
	friend vsm::result<basic_facade<OtherHandle, Traits>> tag_invoke(
		rebind_handle_t<basic_facade<OtherHandle, Traits>>,
		basic_facade&& h,
		auto&&... args)
	{
		vsm_try(new_h, rebind_handle<OtherHandle>(
			static_cast<Handle&&>(h),
			vsm_forward(args)...));

		return vsm::result<basic_facade<OtherHandle, Traits>>(
			vsm::result_value,
			vsm_move(new_h));
	}
#endif
};


struct default_traits
{
	static constexpr bool has_transform_result = true;

	template<typename T>
	static T transform_result(vsm::result<T>&& r)
	{
		return throw_on_error(vsm_move(r));
	}

	template<object Object, optional_multiplexer_handle_for<Object> MultiplexerHandle = void>
	using handle = basic_facade<basic_handle<Object, MultiplexerHandle>, default_traits>;

	template<observer Operation, detached_handle Handle>
	[[nodiscard]] static io_result_t<basic_facade<Handle, default_traits>, Operation> observe(
		basic_facade<Handle, default_traits> const& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a)
	{
		using result_type = io_result_t<basic_facade<Handle, default_traits>, Operation>;

		auto r = blocking_io<Operation>(h, a);
		using value_type = typename decltype(r)::value_type;

		if (!r)
		{
			throw_error(r.error());
		}

		if constexpr (std::is_same_v<value_type, result_type>)
		{
			if constexpr (!std::is_void_v<result_type>)
			{
				return vsm_move(*r);
			}
		}
		else
		{
			return throw_on_error(rebind_handle<result_type>(vsm_move(*r)));
		}
	}

	template<observer Operation, attached_handle Handle>
	[[nodiscard]] static ex::sender auto observe(
		basic_facade<Handle, default_traits> const& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a)
	{
		return io_sender<basic_facade<Handle, default_traits>, Operation>(h, a);
	}
};

struct default_blocking_traits : default_traits
{
	template<object Object, producer Operation>
	static handle<Object> produce(io_parameters_t<Object, Operation> const& a)
	{
		handle<Object> h;
		throw_on_error(blocking_io<Operation>(h, a));
		return h;
	}
};

struct default_sender_traits : default_traits
{
	template<object Object>
	struct _handle_template
	{
		template<multiplexer_handle_for<Object> MultiplexerHandle>
		using type = handle<Object, MultiplexerHandle>;
	};

	template<object Object, producer Operation>
	static ex::sender auto produce(io_parameters_t<Object, Operation> const& a)
	{
		return io_handle_sender<Object, Operation, typename _handle_template<Object>::type>(a);
	}
};

} // namespace allio::detail
