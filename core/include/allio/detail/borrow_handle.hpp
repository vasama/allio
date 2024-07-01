#pragma once

#include <allio/detail/object.hpp>

namespace allio::detail {

template<object Object>
class detached_handle_view
{
public:
	using handle_concept = void;
	using object_type = Object;
	using multiplexer_handle_type = void;

private:
	using native_type = native_handle<Object>;

	native_type const* m_native;

public:
	detached_handle_view()
		: m_native(nullptr)
	{
	}

	template<detached_handle_for<Objct> Handle>
	detached_handle_view(Handle const& handle)
		: m_native(&handle.native())
	{
	}


	[[nodiscard]] native_type const& native() const
	{
		return *m_native;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_native != nullptr;
	}
};

template<detached_handle Handle>
detached_handle_view(Handle const&) -> detached_handle_view<typename Handle::object_type>;


template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
class attached_handle_view
{
public:
	using handle_concept = void;
	using object_type = Object;
	using multiplexer_handle_type = MultiplexerHandle;

private:
	using native_type = native_handle<Object>;
	using multiplexer_type = typename MultiplexerHandle::multiplexer_type;

	native_type const* m_native;
	async_operation_t<multiplexer_type, Object> const* m_connector;
	MultiplexerHandle const* m_multiplexer_handle;

public:
	attached_handle_view()
		: m_native(nullptr)
	{
	}

	template<attached_handle_for<Objct> Handle>
	attached_handle_view(Handle const& handle)
		: m_native(&handle.native())
	{
	}


	[[nodiscard]] native_type const& native() const
	{
		return *m_native;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_native != nullptr;
	}
};

template<attached_handle Handle>
attached_handle_view(Handle const&) -> attached_handle_view<
	typename Handle::object_type,
	typename Handle::multiplexer_handle_type>;


template<bool>
struct _basic_handle_view;

template<>
struct _basic_handle_view<0>
{
	template<typename Object, typename MultiplexerHandle>
	using type = attached_handle_view<Object, MultiplexerHandle>;
};

template<>
struct _basic_handle_view<1>
{
	template<typename Object, typename MultiplexerHandle>
	using type = detached_handle_view<Object>;
};

template<object Object, optional_multiplexer_handle_for<Object> MultiplexerHandle>
using basic_handle_view =
	typename _basic_handle_view<std::is_void_v<MultiplexerHandle>>
		::template type<Object, MultiplexerHandle>;

} // namespace allio::detail
