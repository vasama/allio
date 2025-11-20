#pragma once

#include <allio/detail/object.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/platform.h>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <concepts>

namespace allio::detail {

struct adopt_handle_t
{
	explicit adopt_handle_t() = default;
};
inline constexpr adopt_handle_t adopt_handle{};


template<object Object>
class basic_detached_handle;

template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
class basic_attached_handle;

template<bool IsVoid>
struct _basic_handle;

template<>
struct _basic_handle<1>
{
	template<typename Object, typename MultiplexerHandle>
	using type = basic_detached_handle<Object>;
};

template<>
struct _basic_handle<0>
{
	template<typename Object, typename MultiplexerHandle>
	using type = basic_attached_handle<Object, MultiplexerHandle>;
};

template<object Object, optional_multiplexer_handle_for<Object> MultiplexerHandle>
using basic_handle = typename _basic_handle<std::is_void_v<MultiplexerHandle>>::template type<Object, MultiplexerHandle>;


template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
struct basic_handle_rebind_traits;


template<typename Handle, typename Connector>
struct native_handle_pair
{
	vsm_no_unique_address Handle handle;
	vsm_no_unique_address Connector connector;
};


template<object Object>
class basic_detached_handle
{
public:
	using handle_concept = void;
	using object_type = Object;
	using multiplexer_handle_type = void;
	using native_type = native_handle<Object>;

private:
	native_type m_native;

public:
	template<object OtherObject>
	using rebind_object = basic_detached_handle<OtherObject>;

	template<optional_multiplexer_handle_for<Object> OtherMultiplexerHandle>
	using rebind_multiplexer = basic_handle<Object, OtherMultiplexerHandle>;


	basic_detached_handle()
		requires std::is_default_constructible_v<native_type>
	{
		m_native.flags = handle_flags::none;
	}

	explicit constexpr basic_detached_handle(
		adopt_handle_t,
		std::convertible_to<native_type> auto&& native)
		: m_native(vsm_forward(native))
	{
	}

	basic_detached_handle(basic_detached_handle&& other)
		: m_native(vsm_move(other.m_native))
	{
		other.m_native.flags = handle_flags::none;
	}

	basic_detached_handle& operator=(basic_detached_handle&& other) &
	{
		if (*this)
		{
			close();
		}

		m_native = vsm_move(other.m_native);
		other.m_native.flags = handle_flags::none;

		return *this;
	}

	~basic_detached_handle()
	{
		if (*this)
		{
			close();
		}
	}


	[[nodiscard]] native_type const& native() const
	{
		return m_native;
	}

	[[nodiscard]] explicit operator bool() const
	{
		using object_native_handle_type [[maybe_unused]] = native_handle<object_t>;
		return m_native.object_native_handle_type::flags[object_t::flags::not_null];
	}


	void close()
	{
		if (*this)
		{
			unrecoverable(blocking_io<close_t>(
				m_native,
				no_parameters_t()));

			vsm_assert(!*this);
		}
	}

	[[nodiscard]] native_type release()
	{
		vsm_assert(*this); //PRECONDITION
		native_type const r = vsm_move(m_native);
		m_native.flags = handle_flags::none;
		return r;
	}

private:
	template<multiplexer_handle_for<Object> OtherMultiplexerHandle>
	static vsm::result<basic_attached_handle<Object, OtherMultiplexerHandle>> _rebind(
		basic_detached_handle& h,
		std::convertible_to<OtherMultiplexerHandle> auto&& multiplexer)
	{
		vsm::result<basic_attached_handle<Object, OtherMultiplexerHandle>> r(
			vsm::result_value,
			vsm_forward(multiplexer));

		if (h)
		{
			auto const r_attach = attach_handle(
				vsm_as_const(r->m_multiplexer_handle),
				vsm_as_const(h.m_native),
				r->m_connector);

			if (r_attach)
			{
				r->m_native = vsm_move(h.m_native);
				h.m_native.flags = handle_flags::none;
			}
			else
			{
				r = vsm::unexpected(r_attach.error());
			}
		}

		return r;
	}

	template<multiplexer_handle_for<Object> OtherMultiplexerHandle>
	[[deprecated]] friend vsm::result<basic_attached_handle<Object, OtherMultiplexerHandle>> tag_invoke(
		rebind_handle_t<basic_attached_handle<Object, OtherMultiplexerHandle>>,
		basic_detached_handle&& h,
		std::convertible_to<OtherMultiplexerHandle> auto&& multiplexer)
	{
		return _rebind<OtherMultiplexerHandle>(h, vsm_forward(multiplexer));
	}

	template<operation_c Operation>
	[[deprecated]] friend vsm::result<io_result_t<basic_detached_handle, Operation>> tag_invoke(
		blocking_io_t<Operation>,
		handle_const_t<Operation, basic_detached_handle>& h,
		io_parameters_t<Object, Operation> const& args)
	{
		return blocking_io<Operation>(h.m_native, args);
	}

	template<object OtherObject, multiplexer_handle_for<OtherObject> OtherMultiplexerHandle>
	friend class basic_attached_handle;

	template<object OtherObject, multiplexer_handle_for<OtherObject> OtherMultiplexerHandle>
	friend struct basic_handle_rebind_traits;

	friend handle_traits<basic_detached_handle<Object>>;
};

template<object Object>
struct handle_traits<basic_detached_handle<Object>>
{
	template<operation_c Operation>
	static vsm::result<io_result_t<basic_detached_handle<Object>, Operation>> blocking_io(
		handle_const_t<Operation, basic_detached_handle<Object>>& h,
		io_parameters_t<Object, Operation> const& a)
	{
		return detail::blocking_io<Operation>(h.m_native, a);
	}
};


template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
class basic_attached_handle
{
	using detached_handle_type = basic_detached_handle<Object>;

	using multiplexer_type = typename MultiplexerHandle::multiplexer_type;

public:
	using handle_concept = void;
	using object_type = Object;
	using multiplexer_handle_type = MultiplexerHandle;

	using native_type = native_handle<Object>;
	static_assert(std::is_nothrow_move_constructible_v<native_type>);
	static_assert(std::is_nothrow_move_assignable_v<native_type>);

	using connector_type = async_connector_t<multiplexer_type, Object>;
	static_assert(std::is_nothrow_move_constructible_v<connector_type>);
	static_assert(std::is_nothrow_move_assignable_v<connector_type>);

private:
	native_type m_native;
	vsm_no_unique_address MultiplexerHandle m_multiplexer_handle;
	vsm_no_unique_address connector_type m_connector;

public:
	template<object OtherObject>
		requires multiplexer_handle_for<MultiplexerHandle, OtherObject>
	using rebind_object = basic_attached_handle<OtherObject, MultiplexerHandle>;

	template<optional_multiplexer_handle_for<Object> OtherMultiplexerHandle>
	using rebind_multiplexer = basic_handle<Object, OtherMultiplexerHandle>;

	basic_attached_handle()
		requires
			std::is_default_constructible_v<native_type> &&
			std::is_default_constructible_v<MultiplexerHandle>
		: m_multiplexer_handle()
	{
		m_native.flags = handle_flags::none;
	}

	explicit basic_attached_handle(std::convertible_to<MultiplexerHandle> auto&& multiplexer_handle)
		requires std::is_default_constructible_v<native_type>
		: m_multiplexer_handle(vsm_forward(multiplexer_handle))
	{
		m_native.flags = handle_flags::none;
	}

	//TODO: Adopt should be noexcept
	explicit constexpr basic_attached_handle(
		adopt_handle_t,
		std::convertible_to<MultiplexerHandle> auto&& multiplexer_handle,
		std::convertible_to<native_type> auto&& native,
		std::convertible_to<connector_type> auto&& connector)
		: m_native(vsm_forward(native))
		, m_multiplexer_handle(vsm_forward(multiplexer_handle))
		, m_connector(vsm_forward(connector))
	{
	}

	basic_attached_handle(basic_attached_handle&& other)
		: m_native(vsm_move(other.m_native))
		, m_multiplexer_handle(vsm_move(other.m_multiplexer_handle))
		, m_connector(vsm_move(other.m_connector))
	{
		other.m_native.flags = handle_flags::none;
	}

	basic_attached_handle& operator=(basic_attached_handle&& other) &
	{
		close();

		m_native = vsm_move(other.m_native);
		m_multiplexer_handle = vsm_move(other.m_multiplexer_handle);
		m_connector = vsm_move(other.m_connector);
		other.m_native.flags = handle_flags::none;

		return *this;
	}

	~basic_attached_handle()
	{
		close();
	}


	[[nodiscard]] native_type const& native() const
	{
		return m_native;
	}

	[[nodiscard]] multiplexer_handle_type const& multiplexer() const
	{
		return m_multiplexer_handle;
	}

	[[nodiscard]] connector_type const& connector() const
	{
		return m_connector;
	}

	[[nodiscard]] explicit operator bool() const
	{
		using object_native_handle_type [[maybe_unused]] = native_handle<object_t>;
		return m_native.object_native_handle_type::flags[object_t::flags::not_null];
	}


	void close()
	{
		if (*this)
		{
			//TODO: Handle detach
			unrecoverable(blocking_io<close_t>(
				m_native,
				no_parameters_t()));

			vsm_assert(!*this);
		}
	}

	[[nodiscard]] native_handle_pair<native_type, connector_type> release()
	{
		vsm_assert(*this); //PRECONDITION
		native_handle_pair<native_type, connector_type> r =
		{
			vsm_move(m_native),
			vsm_move(m_connector),
		};
		m_native.flags = handle_flags::none;
		return r;
	}


private:
	static vsm::result<detached_handle_type> _rebind(
		basic_attached_handle& h)
	{
		vsm::result<detached_handle_type> r(vsm::result_value);

		if (h.native_type::flags[object_t::flags::not_null])
		{
			auto const r_detach = detach_handle(
				vsm_as_const(h.m_multiplexer_handle),
				vsm_as_const(h.m_native));

			if (r_detach)
			{
				r->m_native = vsm_move(h.m_native);
				h.m_native.flags = handle_flags::none;
			}
			else
			{
				r = vsm::unexpected(r_detach.error());
			}
		}

		return r;
	}


	template<object OtherObject>
	friend class basic_detached_handle;

	template<object OtherObject, multiplexer_handle_for<OtherObject> OtherMultiplexerHandle>
	friend struct basic_handle_rebind_traits;

	friend handle_traits<basic_attached_handle<Object, MultiplexerHandle>>;
};

template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
struct handle_traits<basic_attached_handle<Object, MultiplexerHandle>>
{
	using _handle_type = basic_attached_handle<Object, MultiplexerHandle>;
	using _multiplexer_type = typename MultiplexerHandle::multiplexer_type;

	template<observer Operation>
	static vsm::result<io_result_t<_handle_type, Operation>> blocking_io(
		_handle_type const& h,
		io_parameters_t<Object, Operation> const& a)
	{
		return detail::blocking_io<Operation>(h.m_native, a);
	}

	template<operation_c Operation>
	static io_result<io_result_t<_handle_type, Operation>> submit_io(
		handle_const_t<Operation, _handle_type>& h,
		async_operation_t<_multiplexer_type, Object, Operation>& s,
		io_parameters_t<Object, Operation> const& a,
		io_handler<_multiplexer_type>& handler)
	{
		return detail::submit_io(
			vsm_as_const(h.m_multiplexer_handle),
			h.m_native,
			h.m_connector,
			s,
			a,
			handler);
	}

	template<operation_c Operation>
	static io_result<io_result_t<_handle_type, Operation>> notify_io(
		handle_const_t<Operation, _handle_type>& h,
		async_operation_t<_multiplexer_type, Object, Operation>& s,
		io_parameters_t<Object, Operation> const& a,
		io_handler<_multiplexer_type>& handler,
		typename _multiplexer_type::io_status_type&& status)
	{
		return detail::notify_io(
			vsm_as_const(h.m_multiplexer_handle),
			h.m_native,
			h.m_connector,
			s,
			a,
			handler,
			vsm_move(status));
	}

	template<operation_c Operation>
	static void cancel_io(
		_handle_type const& h,
		async_operation_t<_multiplexer_type, Object, Operation>& s)
	{
		return detail::cancel_io(
			h.m_multiplexer_handle,
			h.m_native,
			h.m_connector,
			s);
	}
};


template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
struct basic_handle_rebind_traits
{
	using _detached_handle_type = basic_detached_handle<Object>;
	using _attached_handle_type = basic_attached_handle<Object, MultiplexerHandle>;

	static vsm::result<basic_attached_handle<Object, MultiplexerHandle>> rebind(
		basic_detached_handle<Object>&& h,
		std::convertible_to<MultiplexerHandle> auto&& multiplexer)
	{
		vsm::result<basic_attached_handle<Object, MultiplexerHandle>> r(
			vsm::result_value,
			vsm_forward(multiplexer));

		if (h)
		{
			auto const r2 = attach_handle(
				vsm_as_const(r->m_multiplexer_handle),
				vsm_as_const(h.m_native),
				r->m_connector);

			if (r2)
			{
				r->m_native = vsm_move(h.m_native);
				h.m_native.flags = handle_flags::none;
			}
			else
			{
				r = vsm::unexpected(r2.error());
			}
		}

		return r;
	}

	static vsm::result<_detached_handle_type> rebind(_attached_handle_type&& h)
	{
		vsm::result<_detached_handle_type> r(vsm::result_value);

		if (h)
		{
			auto const r2 = detach_handle(
				vsm_as_const(h.m_multiplexer_handle),
				vsm_as_const(h.m_native),
				h.m_connector);

			if (r2)
			{
				r->m_native = vsm_move(h.m_native);
				h.m_native.flags = handle_flags::none;
			}
			else
			{
				r = vsm::unexpected(r2.error());
			}
		}

		return r;
	}
};

template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
struct rebind_traits<basic_detached_handle<Object>, basic_attached_handle<Object, MultiplexerHandle>>
	: basic_handle_rebind_traits<Object, MultiplexerHandle>
{
};

template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
struct rebind_traits<basic_attached_handle<Object, MultiplexerHandle>, basic_detached_handle<Object>>
	: basic_handle_rebind_traits<Object, MultiplexerHandle>
{
};


template<object Object>
vsm_always_inline void verify_handle(native_handle<Object> const& h)
{
	if constexpr (requires { Object::verify_handle(h); })
	{
		vsm_assert(Object::verify_handle(h));
	}
}

} // namespace allio::detail
