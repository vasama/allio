#pragma once

#include <allio/detail/object.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <concepts>

namespace allio::detail {

struct adopt_handle_t {};
inline constexpr adopt_handle_t adopt_handle = {};


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
	static_assert(std::is_default_constructible_v<native_type>);

private:
	native_type m_native = {};

public:
	template<object Object>
	using rebind_object = basic_detached_handle<Object>;

	template<optional_multiplexer_handle_for<Object> OtherMultiplexerHandle>
	using rebind_multiplexer = basic_handle<Object, OtherMultiplexerHandle>;


	basic_detached_handle() = default;

	explicit basic_detached_handle(
		adopt_handle_t,
		std::convertible_to<native_type> auto&& native)
		: m_native(vsm_forward(native))
	{
	}

	basic_detached_handle(basic_detached_handle&& other)
		: m_native(vsm_move(other.m_native))
	{
		other.m_native = {};
	}

	basic_detached_handle& operator=(basic_detached_handle&& other) &
	{
		if (*this)
		{
			close();
		}

		m_native = vsm_move(other.m_native);
		other.m_native = {};

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
		using object_native_handle_type = native_handle<object_t>;
		return m_native.object_native_handle_type::flags[object_t::flags::not_null];
	}


	void close()
	{
		unrecoverable(blocking_io<close_t>(
			*this,
			no_parameters_t()));
	}

	[[nodiscard]] native_type release()
	{
		vsm_assert(*this); //PRECONDITION
		native_type const r = vsm_move(m_native);
		m_native = {};
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
				h.m_native = {};
			}
			else
			{
				r = vsm::unexpected(r_attach.error());
			}
		}

		return r;
	}

	template<multiplexer_handle_for<Object> OtherMultiplexerHandle>
	friend vsm::result<basic_attached_handle<Object, OtherMultiplexerHandle>> tag_invoke(
		rebind_handle_t<basic_attached_handle<Object, OtherMultiplexerHandle>>,
		basic_detached_handle&& h,
		std::convertible_to<OtherMultiplexerHandle> auto&& multiplexer)
	{
		return _rebind<OtherMultiplexerHandle>(h, vsm_forward(multiplexer));
	}

	template<operation_c Operation>
	friend vsm::result<io_result_t<basic_detached_handle, Operation>> tag_invoke(
		blocking_io_t<Operation>,
		handle_const_t<Operation, basic_detached_handle>& h,
		io_parameters_t<Object, Operation> const& args)
	{
		return blocking_io<Operation>(h.m_native, args);
	}

	template<object OtherObject, multiplexer_handle_for<OtherObject> OtherMultiplexerHandle>
	friend class basic_attached_handle;
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
	static_assert(std::is_default_constructible_v<native_type>);

	using connector_type = async_connector_t<multiplexer_type, Object>;
	static_assert(std::is_default_constructible_v<connector_type>);

private:
	native_type m_native = {};
	vsm_no_unique_address MultiplexerHandle m_multiplexer_handle = {};
	vsm_no_unique_address connector_type m_connector;

public:
	template<object Object>
		requires multiplexer_handle_for<MultiplexerHandle, Object>
	using rebind_object = basic_attached_handle<Object, MultiplexerHandle>;

	template<optional_multiplexer_handle_for<Object> OtherMultiplexerHandle>
	using rebind_multiplexer = basic_handle<Object, OtherMultiplexerHandle>;

private:

public:
	basic_attached_handle()
		requires std::is_default_constructible_v<MultiplexerHandle>
		= default;

	explicit basic_attached_handle(std::convertible_to<MultiplexerHandle> auto&& multiplexer_handle)
		: m_multiplexer_handle(vsm_forward(multiplexer_handle))
	{
	}

	explicit basic_attached_handle(
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
		other.m_native = {};
	}

	basic_attached_handle& operator=(basic_attached_handle&& other) &
	{
		close();

		m_native = vsm_move(other.m_native);
		m_multiplexer_handle = vsm_move(other.m_multiplexer_handle);
		m_connector = vsm_move(other.m_connector);
		other.m_native = {};

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
		using object_native_handle_type = native_handle<object_t>;
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
		m_native = {};
		return r;
	}


private:
	static vsm::result<detached_handle_type> _rebind(
		basic_attached_handle& h)
	{
		vsm::result<detached_handle_type> r(vsm::result_value);

		if (h.object_t::native_type::flags[object_t::flags::not_null])
		{
			auto const r_detach = detach_handle(
				vsm_as_const(h.m_multiplexer_handle),
				vsm_as_const(h.m_native));

			if (r_detach)
			{
				r->m_native = vsm_move(h.m_native);
				h.m_native = {};
			}
			else
			{
				r = vsm::unexpected(r_detach.error());
			}
		}

		return r;
	}

	friend vsm::result<detached_handle_type> tag_invoke(
		rebind_handle_t<detached_handle_type>,
		basic_attached_handle&& h)
	{
		return _rebind(h);
	}

	template<observer Operation>
	friend vsm::result<io_result_t<basic_attached_handle, Operation>> tag_invoke(
		blocking_io_t<Operation>,
		basic_attached_handle const& h,
		io_parameters_t<Object, Operation> const& args)
	{
		return blocking_io<Operation>(h.m_native, args);
	}

	template<operation_c Operation>
	friend io_result<io_result_t<basic_attached_handle, Operation>> tag_invoke(
		submit_io_t,
		handle_const_t<Operation, basic_attached_handle>& h,
		async_operation_t<multiplexer_type, Object, Operation>& s,
		io_parameters_t<Object, Operation> const& a,
		io_handler<multiplexer_type>& handler)
	{
		return submit_io(
			vsm_as_const(h.m_multiplexer_handle),
			h.m_native,
			h.m_connector,
			s,
			a,
			handler);
	}

	template<operation_c Operation>
	friend io_result<io_result_t<basic_attached_handle, Operation>> tag_invoke(
		notify_io_t,
		handle_const_t<Operation, basic_attached_handle>& h,
		async_operation_t<multiplexer_type, Object, Operation>& s,
		io_parameters_t<Object, Operation> const& a,
		typename multiplexer_type::io_status_type&& status)
	{
		return notify_io(
			vsm_as_const(h.m_multiplexer_handle),
			h.m_native,
			h.m_connector,
			s,
			a,
			vsm_move(status));
	}

	template<operation_c Operation>
	friend void tag_invoke(
		cancel_io_t,
		basic_attached_handle const& h,
		async_operation_t<multiplexer_type, Object, Operation>& s)
	{
		return cancel_io(
			h.m_multiplexer_handle,
			h.m_native,
			h.m_connector,
			s);
	}


	template<object OtherObject>
	friend class basic_detached_handle;
};

} // namespace allio::detail
