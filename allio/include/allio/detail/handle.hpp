#pragma once

#include <allio/detail/api.hpp>
#include <allio/detail/handle_flags.hpp>
#include <allio/detail/io.hpp>
#include <allio/detail/lifetime.hpp>
#include <allio/detail/parameters.hpp>
#include <allio/detail/parameters2.hpp>
#include <allio/detail/type_list.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/linear.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <cstdint>

namespace allio::detail {

struct object_base_t
{
	struct flags : handle_flags
	{
		flags() = delete;
		static constexpr uint32_t allio_detail_handle_flags_flag_count = 0;
		static constexpr uint32_t allio_detail_handle_flags_flag_limit = 16;
	};

	struct impl_type
	{
		struct flags : handle_flags
		{
			flags() = delete;
			static constexpr uint32_t allio_detail_handle_flags_flag_count = 16;
			static constexpr uint32_t allio_detail_handle_flags_flag_limit = 32;
		};
	};
};

struct object_t : object_base_t
{
	using base_type = object_base_t;

	allio_handle_flags
	(
		not_null,
	);


	struct native_type
	{
		handle_flags flags;
	};

	static bool test_native_handle(native_type const& h)
	{
		return h.flags[flags::not_null];
	}

	static void zero_native_handle(native_type& h)
	{
		h.flags = {};
	}


	struct close_t
	{
		using mutation_tag = consumer_t;

		using handle_type = object_t const;
		using result_type = void;

		using required_params_type = no_parameters_t;
		using optional_params_type = no_parameters_t;
	};

	using operations = type_list<>;


	template<typename H>
	struct abstract_interface
	{
	};

	template<typename H, typename M>
	struct concrete_interface
	{
	};


	void blocking_io() = delete;
};

template<typename M>
struct _basic_multiplexer_handle
{
	class type
	{
		M* m_multiplexer;

	public:
		using multiplexer_type = M;

		type(M& multiplexer)
			: m_multiplexer(&multiplexer)
		{
		}

		operator M&() const
		{
			return *m_multiplexer;
		}

		friend vsm::result<bool> tag_invoke(poll_io_t, type const& self, auto&&... args)
		{
			return poll_io(*self.m_multiplexer, vsm_forward(args)...);
		}
	};
};

template<typename M>
using basic_multiplexer_handle = typename _basic_multiplexer_handle<M>::type;

template<typename M>
auto _multiplexer_handle(M const&)
{
	if constexpr (requires { typename M::multiplexer_tag; })
	{
		static_assert(std::is_void_v<typename M::multiplexer_tag>);

		if constexpr (requires { typename M::handle_type; })
		{
			return vsm::tag<typename M::handle_type>();
		}
		else
		{
			return vsm::tag<basic_multiplexer_handle<M>>();
		}
	}
	else
	{
		return vsm::tag<M>();
	}
}

template<typename M>
using multiplexer_handle_t = typename decltype(_multiplexer_handle(vsm_declval(M)))::type;

template<typename M>
concept multiplexer = requires { requires std::is_void_v<typename M::multiplexer_tag>; };

template<typename M, typename H>
concept multiplexer_for = true;

template<typename M>
concept multiplexer_handle = true;

template<typename M, typename H>
concept multiplexer_handle_for = true;


struct adopt_handle_t {};

template<typename Handle>
class blocking_handle;

template<typename Handle, multiplexer_handle_for<Handle> MultiplexerHandle>
class async_handle;

template<typename Handle>
class abstract_handle
	: public Handle::template abstract_interface<abstract_handle<Handle>>
{
public:
	using native_type = typename Handle::native_type;

protected:
	native_type m_native;

public:
	[[nodiscard]] native_type const& native() const
	{
		vsm_assert(*this); //PRECONDITION
		return m_native;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_native.object_t::native_type::flags[Handle::flags::not_null];
	}

protected:
	allio_detail_default_lifetime(abstract_handle);

	template<std::convertible_to<native_type> N>
	explicit abstract_handle(adopt_handle_t, N&& native)
		: m_native(vsm_forward(native))
	{
	}

	[[nodiscard]] native_type& _native()
	{
		return m_native;
	}

	[[nodiscard]] native_type const& _native() const
	{
		return m_native;
	}

	template<typename H>
	friend class blocking_handle;

	template<typename H, multiplexer_handle_for<H> M>
	friend class async_handle;

	template<observer O>
	friend vsm::result<io_result_t<O, Handle>> tag_invoke(
		blocking_io_t<O>,
		abstract_handle const& h,
		io_parameters_t<O> const& args)
	{
		return Handle::blocking_io(
			O(),
			h.m_native,
			args);
	}
};

template<typename Handle>
class blocking_handle
	: public abstract_handle<Handle>
	, public Handle::template concrete_interface<blocking_handle<Handle>, void>
{
	using abstract_handle_type = abstract_handle<Handle>;

public:
	using handle_tag_type = Handle;

	blocking_handle()
		: abstract_handle_type{}
	{
	}

	template<std::convertible_to<typename Handle::native_type> N>
	explicit blocking_handle(adopt_handle_t, N&& native)
		: abstract_handle_type(adopt_handle_t(), vsm_forward(native))
	{
	}

	blocking_handle(blocking_handle&& other)
		: abstract_handle_type(static_cast<abstract_handle_type&&>(other))
	{
		Handle::zero_native_handle(other.m_native);
	}

	blocking_handle& operator=(blocking_handle&& other) &
	{
		if (*this)
		{
			close();
		}

		static_cast<abstract_handle_type&>(*this) = static_cast<abstract_handle_type&&>(other);
		Handle::zero_native_handle(other.m_native);

		return *this;
	}

	~blocking_handle()
	{
		if (*this)
		{
			close();
		}
	}


	[[nodiscard]] vsm::result<typename abstract_handle_type::native_type> release_native_handle()
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		vsm::result<typename abstract_handle_type::native_type> r(
			vsm::result_value,
			vsm_move(abstract_handle_type::m_native));
		Handle::zero_native_handle(abstract_handle_type::m_native);
		return r;
	}

#if 0 //TODO: Probably no longer needed with adopt_handle_t added.
	[[nodiscard]] vsm::result<void> set_native_handle(typename abstract_handle_type::native_type&& handle)&
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		if (!Handle::test_native_handle(vsm_as_const(handle)))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		abstract_handle_type::m_native = vsm_move(handle);

		return {};
	}
#endif


	void close()
	{
		unrecoverable(blocking_io<object_t::close_t>(
			*this,
			io_args<object_t::close_t>()()));
	}


	template<typename Multiplexer>
		//requires multiplexer_handle_for<multiplexer_handle_t<Multiplexer>>
	[[nodiscard]] vsm::result<async_handle<Handle, multiplexer_handle_t<Multiplexer>>> via(Multiplexer&& multiplexer) &&
	{
		vsm::result<async_handle<Handle, multiplexer_handle_t<Multiplexer>>> r(
			vsm::result_value,
			vsm_forward(multiplexer));

		if (*this)
		{
			vsm_try_void(r->_set_handle(static_cast<abstract_handle_type&&>(*this)));
		}

		return r;
	}

private:
	template<mutation O>
	friend vsm::result<io_result_t<O, Handle>> tag_invoke(
		blocking_io_t<O>,
		blocking_handle& h,
		io_parameters_t<O> const& args)
	{
		return Handle::blocking_io(
			O(),
			h.abstract_handle_type::m_native,
			args);
	}
};

template<typename O, typename H>
vsm::result<io_result_t<O, H>> generic_io(blocking_handle<H>& h, io_parameters_t<O> const& args)
{
	return blocking_io<O>(h, args);
}

template<typename O, typename H>
vsm::result<io_result_t<O, H>> generic_io(blocking_handle<H> const& h, io_parameters_t<O> const& args)
{
	return blocking_io<O>(h, args);
}


template<typename Handle, multiplexer_handle_for<Handle> MultiplexerHandle>
class async_handle
	: public abstract_handle<Handle>
	, public Handle::template concrete_interface<async_handle<Handle, MultiplexerHandle>, MultiplexerHandle>
{
	using abstract_handle_type = abstract_handle<Handle>;
	using concrete_handle_type = blocking_handle<Handle>;

	using multiplexer_type = typename MultiplexerHandle::multiplexer_type;

public:
	using handle_tag_type = Handle;

	using multiplexer_handle_type = MultiplexerHandle;
	using connector_type = connector_t<multiplexer_type, Handle>;
	static_assert(std::is_default_constructible_v<connector_type>);

private:
	vsm_no_unique_address MultiplexerHandle m_multiplexer_handle;
	vsm_no_unique_address connector_type m_connector;

public:
	async_handle()
		requires std::is_default_constructible_v<MultiplexerHandle>
		: abstract_handle_type{}
		, m_multiplexer_handle{}
	{
	}

	template<std::convertible_to<MultiplexerHandle> M = MultiplexerHandle>
	async_handle(M&& multiplexer_handle)
		: abstract_handle_type{}
		, m_multiplexer_handle(vsm_forward(multiplexer_handle))
	{
	}

	template<
		std::convertible_to<typename Handle::native_type> N,
		std::convertible_to<connector_type> C,
		std::convertible_to<MultiplexerHandle> M = MultiplexerHandle>
	explicit async_handle(adopt_handle_t, N&& native, M&& multiplexer_handle, C&& connector)
		: abstract_handle_type(adopt_handle_t(), vsm_forward(native))
		, m_multiplexer_handle(vsm_forward(multiplexer_handle))
		, m_connector(vsm_forward(connector))
	{
	}

	async_handle(async_handle&& other)
		: abstract_handle_type(static_cast<abstract_handle_type&&>(other))
		, m_multiplexer_handle(vsm_move(other.m_multiplexer_handle))
		, m_connector(vsm_move(other.m_connector))
	{
		Handle::zero_native_handle(other.m_native);
	}

	async_handle& operator=(async_handle&& other) &
	{
		if (*this)
		{
			close();
		}

		static_cast<abstract_handle_type&>(*this) = static_cast<abstract_handle_type&&>(other);
		m_multiplexer_handle = vsm_move(other.m_multiplexer_handle);
		m_connector = vsm_move(other.m_connector);
		Handle::zero_native_handle(other.m_native);

		return *this;
	}

	~async_handle()
	{
		if (*this)
		{
			close();
		}
	}


	[[nodiscard]] multiplexer_handle_type const& multiplexer() const
	{
		return m_multiplexer_handle;
	}

	[[nodiscard]] connector_type const& connector() const
	{
		return m_connector;
	}


	void close()
	{
		//TODO: Handle detach
		vsm_verify(Handle::blocking_io(
			object_t::close_t(),
			abstract_handle_type::m_native,
			io_parameters_t<object_t::close_t>{}));
	}

private:
	vsm::result<void> _set_handle(abstract_handle_type&& h)
	{
		vsm_assert(!*this);

		auto& h_native = h._native();

		vsm_try_void(attach_handle(
			vsm_as_const(m_multiplexer_handle),
			vsm_as_const(h_native),
			m_connector));

		abstract_handle_type::m_native = vsm_move(h_native);
		Handle::zero_native_handle(h_native);

		return {};
	}


	template<typename O>
	friend auto tag_invoke(
		submit_io_t<O>,
		handle_cv<O, async_handle>& h,
		operation_t<multiplexer_type, Handle, O>& s)
	{
		return submit_io<O>(
			vsm_as_const(h.m_multiplexer_handle),
			h._native(),
			h.m_connector,
			s);
	}

	template<typename O>
	friend io_result2<io_result_t<O, Handle, MultiplexerHandle>> tag_invoke(
		notify_io_t<O>,
		handle_cv<O, async_handle>& h,
		operation_t<multiplexer_type, Handle, O>& s,
		io_status const status)
	{
		return notify_io<O>(
			vsm_as_const(h.m_multiplexer_handle),
			h._native(),
			h.m_connector,
			s,
			status);
	}

	template<typename O>
	friend io_result2<io_result_t<O, Handle, MultiplexerHandle>> tag_invoke(
		cancel_io_t<O>,
		async_handle const& h,
		operation_t<multiplexer_type, Handle, O>& s)
	{
		return cancel_io<O>(
			h.m_multiplexer_handle,
			h._native(),
			h.m_connector,
			s);
	}


	friend blocking_handle<Handle>;
};


template<bool IsVoid>
struct _basic_handle;

template<>
struct _basic_handle<1>
{
	template<typename H, typename M>
	using type = blocking_handle<H>;
};

template<>
struct _basic_handle<0>
{
	template<typename H, typename M>
	using type = async_handle<H, M>;
};

template<typename Handle, typename MultiplexerHandle>
using basic_handle = typename _basic_handle<std::is_void_v<MultiplexerHandle>>::template type<Handle, MultiplexerHandle>;

} // namespace allio::detail
