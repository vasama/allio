#pragma once

#include <allio/detail/expected.hpp>
#include <allio/detail/lifetime.hpp>
#include <allio/detail/object.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <concepts>

namespace allio::detail {

template<typename Handle>
concept handle = requires (Handle const& h)
{
	typename Handle::handle_concept;
	requires object<typename Handle::object_type>;
	typename Handle::io_traits_type;
	requires optional_multiplexer_handle<typename Handle::multiplexer_handle_type>;
	{ h.native() } -> std::same_as<typename Handle::object_type::native_type const&>;
};

template<typename Handle, typename Object>
concept handle_for = handle<Handle> && std::is_same_v<typename Handle::object_type, Object>;


struct adopt_handle_t {};
inline constexpr adopt_handle_t adopt_handle = {};


template<object Object, typename IoTraits>
class basic_detached_handle;

template<object Object, typename IoTraits, multiplexer_handle_for<Object> MultiplexerHandle>
class basic_attached_handle;

template<bool IsVoid>
struct _basic_handle;

template<>
struct _basic_handle<1>
{
	template<typename Object, typename IoTraits, typename MultiplexerHandle>
	using type = basic_detached_handle<Object, IoTraits>;
};

template<>
struct _basic_handle<0>
{
	template<typename Object, typename IoTraits, typename MultiplexerHandle>
	using type = basic_attached_handle<Object, IoTraits, MultiplexerHandle>;
};

template<object Object, typename IoTraits, optional_multiplexer_handle_for<Object> MultiplexerHandle>
using basic_handle = typename _basic_handle<std::is_void_v<MultiplexerHandle>>::template type<Object, MultiplexerHandle>;


using no_io_traits = expected_error_traits;

struct rebind_handle_tag {};

template<object Object>
class abstract_handle
	: public Object::template abstract_interface<abstract_handle<Object>>
{
public:
	using object_type = Object;
	using native_type = typename Object::native_type;
	static_assert(std::is_default_constructible_v<native_type>);

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
		return m_native.object_t::native_type::flags[Object::flags::not_null];
	}

protected:
	allio_detail_default_lifetime(abstract_handle);

	explicit abstract_handle(
		adopt_handle_t,
		std::convertible_to<native_type> auto&& native)
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

	template<observer Operation>
	friend vsm::result<io_result_t<Object, Operation>> tag_invoke(
		blocking_io_t<Object, Operation>,
		abstract_handle const& h,
		io_parameters_t<Object, Operation> const& args)
	{
		return blocking_io<Object, Operation>(h.m_native, args);
	}

	template<object OtherObject, typename OtherIoTraits>
	friend class basic_detached_handle;

	template<object OtherObject, typename OtherIoTraits, multiplexer_handle_for<OtherObject> MultiplexerHandle>
	friend class basic_attached_handle;
};

template<object Object, typename IoTraits>
class basic_detached_handle final
	: public abstract_handle<Object>
	, public Object::template concrete_interface<basic_detached_handle<Object, IoTraits>>
{
	using abstract_handle_type = abstract_handle<Object>;

public:
	using handle_concept = void;

	using io_traits_type = IoTraits;
	using multiplexer_handle_type = void;

	template<object NewObject>
	using rebind_object = basic_detached_handle<NewObject, IoTraits>;

	template<optional_multiplexer_handle_for<Object> NewMultiplexerHandle>
	using rebind_multiplexer = basic_handle<Object, IoTraits, NewMultiplexerHandle>;


	using native_type = typename Object::native_type;


	basic_detached_handle()
		: abstract_handle_type{}
	{
	}

	explicit basic_detached_handle(
		adopt_handle_t,
		std::convertible_to<native_type> auto&& native)
		: abstract_handle_type(adopt_handle, vsm_forward(native))
	{
	}

	template<typename OtherIoTraits>
	explicit basic_detached_handle(rebind_handle_tag, basic_detached_handle<Object, OtherIoTraits>&& other)
		: abstract_handle_type(static_cast<abstract_handle_type&&>(other))
	{
		other.m_native = {};
	}

	basic_detached_handle(basic_detached_handle&& other)
		: abstract_handle_type(static_cast<abstract_handle_type&&>(other))
	{
		other.m_native = {};
	}

	basic_detached_handle& operator=(basic_detached_handle&& other) &
	{
		if (*this)
		{
			close();
		}

		static_cast<abstract_handle_type&>(*this) = static_cast<abstract_handle_type&&>(other);
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


	void close()
	{
		unrecoverable(blocking_io<Object, close_t>(
			*this,
			io_parameters_t<Object, close_t>{}));
	}

	[[nodiscard]] native_type release()
	{
		vsm_assert(*this); //PRECONDITION
		vsm::result<native_type> r(
			vsm::result_value,
			vsm_move(abstract_handle_type::m_native));
		abstract_handle_type::m_native = {};
		return r;
	}


	template<multiplexer_for<Object> Multiplexer>
	[[nodiscard]] auto via(Multiplexer& multiplexer) &&
	{
		return vsm_move(*this).template via<multiplexer_handle_t<Multiplexer>>(multiplexer);
	}

	#if 0
	template<typename MultiplexerHandle>
	[[nodiscard]] auto via(MultiplexerHandle&& multiplexer_handle) &&
		requires multiplexer_handle_for<std::remove_cvref_t<MultiplexerHandle>, Object>
	{
		return vsm_move(*this).template via<std::remove_cvref_t<MultiplexerHandle>>(vsm_forward(multiplexer));
	}
	#endif

	template<multiplexer_handle_for<Object> MultiplexerHandle>
	[[nodiscard]] auto via(std::convertible_to<MultiplexerHandle> auto&& multiplexer) &&
	{
		return io_traits_type::unwrap_result(_via<MultiplexerHandle>(vsm_forward(multiplexer)));
	}

private:
	template<multiplexer_handle_for<Object> MultiplexerHandle>
	[[nodiscard]] vsm::result<basic_attached_handle<Object, IoTraits, MultiplexerHandle>> _via(
		std::convertible_to<MultiplexerHandle> auto&& multiplexer)
	{
		vsm::result<basic_attached_handle<Object, IoTraits, MultiplexerHandle>> r(
			vsm::result_value,
			vsm_forward(multiplexer));

		if (*this)
		{
			vsm_try_void(r->_set_handle(static_cast<abstract_handle_type&&>(*this)));
		}

		return r;
	}

	template<typename OtherIoTraits>
	friend vsm::result<basic_detached_handle<Object, OtherIoTraits>> tag_invoke(
		rebind_handle_t<basic_detached_handle<Object, OtherIoTraits>>,
		basic_detached_handle&& h)
	{
		return vsm::result<basic_detached_handle<Object, OtherIoTraits>>(
			vsm::result_value,
			rebind_handle_tag(),
			vsm_move(h));
	}

	template<typename OtherIoTraits, multiplexer_handle_for<Object> OtherMultiplexerHandle>
	friend vsm::result<basic_attached_handle<Object, OtherIoTraits, OtherMultiplexerHandle>> tag_invoke(
		rebind_handle_t<basic_attached_handle<Object, OtherIoTraits, OtherMultiplexerHandle>>,
		basic_detached_handle&& h,
		std::convertible_to<OtherMultiplexerHandle> auto&& multiplexer)
	{
		return vsm_move(h).template via<OtherMultiplexerHandle>(vsm_forward(multiplexer));
	}

	template<mutation Operation>
	friend vsm::result<io_result_t<Object, Operation>> tag_invoke(
		blocking_io_t<Object, Operation>,
		basic_detached_handle& h,
		io_parameters_t<Object, Operation> const& args)
	{
		return blocking_io<Object, Operation>(h.m_native, args);
	}

	template<object OtherObject, typename OtherIoTraits, multiplexer_handle_for<OtherObject> MultiplexerHandle>
	friend class basic_attached_handle;
};


template<typename Native, typename Connector>
struct native_pair
{
	vsm_no_unique_address Native native;
	vsm_no_unique_address Connector connector;
};

template<object Object, typename IoTraits, multiplexer_handle_for<Object> MultiplexerHandle>
class basic_attached_handle final
	: public abstract_handle<Object>
	, public Object::template concrete_interface<basic_attached_handle<Object, IoTraits, MultiplexerHandle>>
{
	using abstract_handle_type = abstract_handle<Object>;
	using detached_handle_type = basic_detached_handle<Object, IoTraits>;

	using multiplexer_type = typename MultiplexerHandle::multiplexer_type;

public:
	using handle_concept = void;

	using io_traits_type = IoTraits;
	using multiplexer_handle_type = MultiplexerHandle;

	template<object NewObject>
		requires multiplexer_handle_for<MultiplexerHandle, NewObject>
	using rebind_object = basic_attached_handle<NewObject, IoTraits, MultiplexerHandle>;

	template<optional_multiplexer_handle_for<Object> NewMultiplexerHandle>
	using rebind_multiplexer = basic_handle<Object, IoTraits, NewMultiplexerHandle>;


	using native_type = typename Object::native_type;
	using connector_type = async_connector_t<multiplexer_type, Object>;
	static_assert(std::is_default_constructible_v<connector_type>);

private:
	vsm_no_unique_address MultiplexerHandle m_multiplexer_handle;
	vsm_no_unique_address connector_type m_connector;

public:
	basic_attached_handle()
		requires std::is_default_constructible_v<MultiplexerHandle>
		: abstract_handle_type{}
		, m_multiplexer_handle{}
	{
	}

	explicit basic_attached_handle(std::convertible_to<MultiplexerHandle> auto&& multiplexer_handle)
		: abstract_handle_type{}
		, m_multiplexer_handle(vsm_forward(multiplexer_handle))
	{
	}

	explicit basic_attached_handle(
		adopt_handle_t,
		std::convertible_to<MultiplexerHandle> auto&& multiplexer_handle,
		std::convertible_to<native_type> auto&& native,
		std::convertible_to<connector_type> auto&& connector)
		: abstract_handle_type(adopt_handle, vsm_forward(native))
		, m_multiplexer_handle(vsm_forward(multiplexer_handle))
		, m_connector(vsm_forward(connector))
	{
	}

	template<typename OtherIoTraits>
	explicit basic_attached_handle(basic_attached_handle<Object, OtherIoTraits, MultiplexerHandle>&& other)
		: abstract_handle_type(static_cast<abstract_handle_type&&>(other))
		, m_multiplexer_handle(vsm_move(other.m_multiplexer_handle))
		, m_connector(vsm_move(other.m_connector))
	{
		other.m_native = {};
	}

	basic_attached_handle(basic_attached_handle&& other)
		: abstract_handle_type(static_cast<abstract_handle_type&&>(other))
		, m_multiplexer_handle(vsm_move(other.m_multiplexer_handle))
		, m_connector(vsm_move(other.m_connector))
	{
		other.m_native = {};
	}

	basic_attached_handle& operator=(basic_attached_handle&& other) &
	{
		close();

		static_cast<abstract_handle_type&>(*this) = static_cast<abstract_handle_type&&>(other);
		m_multiplexer_handle = vsm_move(other.m_multiplexer_handle);
		m_connector = vsm_move(other.m_connector);
		other.m_native = {};

		return *this;
	}

	~basic_attached_handle()
	{
		close();
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
		if (*this)
		{
			//TODO: Handle detach
			vsm_verify(blocking_io<Object, close_t>(
				abstract_handle_type::m_native,
				io_parameters_t<Object, close_t>()));
		}
	}

	[[nodiscard]] native_pair<native_type, connector_type> release()
	{
		vsm_assert(*this); //PRECONDITION
		native_pair<native_type, connector_type> r =
		{
			vsm_move(abstract_handle_type::m_native),
			vsm_move(m_connector),
		};
		abstract_handle_type::m_native = {};
		return r;
	}

	[[nodiscard]] auto detach() &&
	{
		return io_traits_type::unwrap_result(_detach());
	}

private:
	[[nodiscard]] vsm::result<void> _set_handle(abstract_handle_type&& h)
	{
		vsm_assert(!*this);

		auto& h_native = h._native();

		vsm_try_void(attach_handle(
			vsm_as_const(m_multiplexer_handle),
			vsm_as_const(h_native),
			m_connector));

		abstract_handle_type::m_native = vsm_move(h_native);
		h_native = {};

		return {};
	}

	[[nodiscard]] vsm::result<detached_handle_type> _detach()
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}
	
		vsm_try_void(detach_handle(
			vsm_as_const(m_multiplexer_handle),
			vsm_as_const(abstract_handle_type::m_native),
			m_connector));

		vsm::result<detached_handle_type> r(
			vsm::result_value,
			adopt_handle,
			abstract_handle_type::m_native);

		abstract_handle_type::m_native = {};

		return r;
	}

	friend vsm::result<detached_handle_type> tag_invoke(
		rebind_handle_t<detached_handle_type>,
		basic_attached_handle&& h)
	{
		return vsm_move(h).detach();
	}

	template<typename OtherIoTraits>
	friend vsm::result<basic_attached_handle<Object, OtherIoTraits, MultiplexerHandle>> tag_invoke(
		rebind_handle_t<basic_attached_handle<Object, OtherIoTraits, MultiplexerHandle>>,
		basic_attached_handle&& h)
	{
		return vsm::result<basic_attached_handle<Object, OtherIoTraits, MultiplexerHandle>>(
			vsm::result_value,
			rebind_handle_tag(),
			vsm_move(h));
	}

	template<operation_c Operation>
	friend io_result<io_result_t<Object, Operation, MultiplexerHandle>> tag_invoke(
		submit_io_t,
		handle_const_t<Operation, basic_attached_handle>& h,
		async_operation_t<multiplexer_type, Object, Operation>& s,
		io_parameters_t<Object, Operation> const& args,
		io_handler<multiplexer_type>& handler)
	{
		return submit_io(
			vsm_as_const(h.m_multiplexer_handle),
			h._native(),
			h.m_connector,
			s,
			args,
			handler);
	}

	template<operation_c Operation>
	friend io_result<io_result_t<Object, Operation, MultiplexerHandle>> tag_invoke(
		notify_io_t,
		handle_const_t<Operation, basic_attached_handle>& h,
		async_operation_t<multiplexer_type, Object, Operation>& s,
		io_parameters_t<Object, Operation> const& args,
		typename multiplexer_type::io_status_type&& status)
	{
		return notify_io(
			vsm_as_const(h.m_multiplexer_handle),
			h._native(),
			h.m_connector,
			s,
			args,
			vsm_move(status));
	}

	template<operation_c Operation>
	friend void tag_invoke(
		cancel_io_t,
		basic_attached_handle const& h,
		async_operation_t<multiplexer_type, Object, Operation>& s)
	{
		cancel_io(
			h.m_multiplexer_handle,
			h._native(),
			h.m_connector,
			s);
	}


	template<object OtherObject, typename OtherIoTraits>
	friend class basic_detached_handle;
};

} // namespace allio::detail
