#pragma once

#include <allio/detail/handle.hpp>

namespace allio::detail {

template<handle Handle, typename Traits, typename UserTraits>
struct _rebind_facade_traits
{
	using type = UserTraits;
};

template<detached_handle Handle, typename Traits>
	requires requires { typename Traits::detached_traits; }
struct _rebind_facade_traits<Handle, Traits, void>
{
	using type = typename Traits::detached_traits;
};

template<attached_handle Handle, typename Traits>
	requires requires { typename Traits::attached_traits; }
struct _rebind_facade_traits<Handle, Traits, void>
{
	using type = typename Traits::attached_traits;
};

template<handle Handle, typename Traits, typename UserTraits>
using rebind_facade_traits = typename _rebind_facade_traits<Handle, Traits, UserTraits>::type;

template<handle Handle, typename Traits>
class basic_facade;

template<typename Handle, typename InnerTraits, typename OuterTraits>
class basic_facade<basic_facade<Handle, InnerTraits>, OuterTraits>
{
	static_assert(sizeof(Handle) == 0);
};

template<detached_handle Handle, typename Traits>
class basic_facade<Handle, Traits>
	: public Handle
	, public Handle::object_type::template facade<basic_facade<Handle, Traits>, Traits>
{
	static_assert(!requires { typename Handle::facade_concept; });

	template<typename OtherHandle, typename UserTraits>
	using rebind = basic_facade<OtherHandle, rebind_facade_traits<OtherHandle, Traits, UserTraits>>;

public:
	using handle_type = Handle;
	using traits_type = Traits;

	using facade_concept = void;

	template<object OtherObject>
	using rebind_object = basic_facade<typename Handle::template rebind_object<OtherObject>, Traits>;

	template<optional_multiplexer_handle_for<typename Handle::object_type> OtherMultiplexerHandle>
	using rebind_multiplexer = basic_facade<typename Handle::template rebind_multiplexer<OtherMultiplexerHandle>, Traits>;


	using Handle::Handle;

	explicit constexpr basic_facade(Handle&& handle)
		: Handle(vsm_move(handle))
	{
	}

	template<typename UserTraits = void, multiplexer_for<typename Handle::object_type> Multiplexer>
	[[nodiscard]] auto via(Multiplexer& multiplexer) &&
	{
		return _via<UserTraits, multiplexer_handle_t<Multiplexer>>(multiplexer);
	}

	template<typename UserTraits = void, typename MultiplexerHandle>
		requires multiplexer_handle_for<std::remove_cvref_t<MultiplexerHandle>, typename Handle::object_type>
	[[nodiscard]] auto via(MultiplexerHandle&& multiplexer) &&
	{
		return _via<UserTraits, std::remove_cvref_t<MultiplexerHandle>>(vsm_forward(multiplexer));
	}

	template<typename UserTraits = void, multiplexer_handle_for<typename Handle::object_type> MultiplexerHandle>
	[[nodiscard]] auto via(std::convertible_to<MultiplexerHandle> auto&& multiplexer) &&
	{
		return _via<UserTraits, MultiplexerHandle>(vsm_forward(multiplexer));
	}

private:
	template<typename UserTraits = void, multiplexer_handle_for<typename Handle::object_type> MultiplexerHandle>
	[[nodiscard]] auto _via(std::convertible_to<MultiplexerHandle> auto&& multiplexer)
	{
		using new_handle_type = typename Handle::template rebind_multiplexer<MultiplexerHandle>;

		auto r = rebind_handle<rebind<new_handle_type, UserTraits>>(
			vsm_move(*this),
			vsm_forward(multiplexer));

		if constexpr (Traits::has_transform_result)
		{
			return Traits::transform_result(vsm_move(r));
		}
		else
		{
			return r;
		}
	}

	[[deprecated]] friend vsm::result<basic_facade> tag_invoke(
		rebind_handle_t<basic_facade>,
		Handle&& h)
	{
		return vsm::result<basic_facade>(vsm::result_value, vsm_move(h));
	}

	[[deprecated]] friend vsm::result<Handle> tag_invoke(
		rebind_handle_t<Handle>,
		basic_facade&& h)
	{
		return vsm::result<Handle>(vsm::result_value, vsm_move(h));
	}

	[[deprecated]] friend vsm::result<basic_facade> tag_invoke(
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
};

template<detached_handle Handle, typename Traits>
struct handle_traits<basic_facade<Handle, Traits>> : handle_traits<Handle>
{
};

template<attached_handle Handle, typename Traits>
class basic_facade<Handle, Traits>
	: public Handle
	, public Handle::object_type::template facade<basic_facade<Handle, Traits>, Traits>
{
	static_assert(!requires { typename Handle::facade_concept; });

	template<typename OtherHandle, typename UserTraits>
	using rebind = basic_facade<OtherHandle, rebind_facade_traits<OtherHandle, Traits, UserTraits>>;

public:
	using handle_type = Handle;
	using traits_type = Traits;

	using facade_concept = void;

	template<object OtherObject>
	using rebind_object = basic_facade<typename Handle::template rebind_object<OtherObject>, Traits>;

	template<optional_multiplexer_handle_for<typename Handle::object_type> OtherMultiplexerHandle>
	using rebind_multiplexer = basic_facade<typename Handle::template rebind_multiplexer<OtherMultiplexerHandle>, Traits>;


	using Handle::Handle;

	explicit constexpr basic_facade(Handle&& handle)
		: Handle(vsm_move(handle))
	{
	}

	template<typename UserTraits = void>
	[[nodiscard]] auto detach() &&
	{
		using new_handle_type = typename Handle::template rebind_multiplexer<void>;
		auto r = rebind_handle<rebind<new_handle_type, UserTraits>>(vsm_move(*this));

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
	[[deprecated]] friend vsm::result<basic_facade> tag_invoke(
		rebind_handle_t<basic_facade>,
		Handle&& h)
	{
		return vsm::result<basic_facade>(vsm::result_value, vsm_move(h));
	}

	[[deprecated]] friend vsm::result<Handle> tag_invoke(
		rebind_handle_t<Handle>,
		basic_facade&& h)
	{
		return vsm::result<Handle>(vsm::result_value, vsm_move(h));
	}

	[[deprecated]] friend vsm::result<basic_facade> tag_invoke(
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
};

template<attached_handle Handle, typename Traits>
struct handle_traits<basic_facade<Handle, Traits>> : handle_traits<Handle>
{
};


template<typename H, typename T>
struct rebind_traits<H, basic_facade<H, T>>
{
	static vsm::result<basic_facade<H, T>> rebind(vsm::any_cvref_of<H> auto&& h)
	{
		return basic_facade<H, T>(vsm_forward(h));
	}
};

template<typename H, typename T>
struct rebind_traits<basic_facade<H, T>, H>
{
	static vsm::result<H> rebind(vsm::any_cvref_of<basic_facade<H, T>> auto&& h)
	{
		H&& h2 = vsm_forward(h);
		return vsm_forward(h2);
	}
};

template<typename H, typename T, typename U>
struct rebind_traits<U, basic_facade<H, T>>
{
	static vsm::result<basic_facade<H, T>> rebind(vsm::any_cvref_of<U> auto&& h, auto&&... args)
	{
		vsm_try(new_h, rebind_handle<H>(
			vsm_forward(h),
			vsm_forward(args)...));

		return vsm::result<basic_facade<H, T>>(
			vsm::result_value,
			vsm_move(new_h));
	}
};

#if 0
template<typename H, typename T, typename U>
struct rebind_traits<basic_facade<H, T>, U>
{
	static vsm::result<U> rebind(vsm::any_cvref_of<basic_facade<H, T>> auto&& h, auto&&... args)
	{
		H&& h2 = vsm_forward(h);
		return rebind_handle<U>(vsm_forward(h2), vsm_forward(args)...);
	}
};
#endif

template<typename H1, typename T1, typename H2, typename T2>
struct rebind_traits<basic_facade<H1, T1>, basic_facade<H2, T2>>
{
	static vsm::result<basic_facade<H2, T2>> rebind(
		vsm::any_cvref_of<basic_facade<H1, T1>> auto&& h,
		auto&&... args)
	{
		H1&& h2 = vsm_forward(h);

		vsm_try(new_h, rebind_handle<H2>(
			vsm_forward(h2),
			vsm_forward(args)...));

		return vsm::result<basic_facade<H2, T2>>(
			vsm::result_value,
			vsm_move(new_h));
	}
};

} // namespace allio::detail
