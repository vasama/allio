#pragma once

#include <allio/detail/multiplexer.hpp>

#include <concepts>

namespace allio::detail {

struct object_t;

template<typename Object>
concept object = true; //TODO: std::derived_from<Object, object_t>;

template<object Object>
struct native_handle;


template<typename Multiplexer, typename Object>
concept _multiplexer_for = object<Object>; //TODO

template<typename Multiplexer, typename Object>
concept multiplexer_for =
	multiplexer<Multiplexer> &&
	_multiplexer_for<Multiplexer, Object>;

template<typename Multiplexer, typename Object>
concept optional_multiplexer_for =
	std::is_void_v<Multiplexer> ||
	multiplexer_for<Multiplexer, Object>;

template<typename MultiplexerHandle, typename Object>
concept multiplexer_handle_for =
	multiplexer_handle<MultiplexerHandle> &&
	_multiplexer_for<typename MultiplexerHandle::multiplexer_type, Object>;

template<typename MultiplexerHandle, typename Object>
concept optional_multiplexer_handle_for =
	std::is_void_v<MultiplexerHandle> ||
	multiplexer_handle_for<MultiplexerHandle, Object>;


template<typename Handle>
concept handle = requires (Handle const& h)
{
	typename Handle::handle_concept;
	requires object<typename Handle::object_type>;
	requires optional_multiplexer_handle<typename Handle::multiplexer_handle_type>;
	{ h.native() } -> std::same_as<native_handle<typename Handle::object_type> const&>;
};

template<typename Handle, typename Object>
concept handle_for = handle<Handle> && std::derived_from<typename Handle::object_type, Object>;

template<typename Handle>
concept detached_handle = handle<Handle> && std::is_void_v<typename Handle::multiplexer_handle_type>;

template<typename Handle, typename Object>
concept detached_handle_for = detached_handle<Handle> && handle_for<Handle, Object>;

template<typename Handle>
concept attached_handle = handle<Handle> && !std::is_void_v<typename Handle::multiplexer_handle_type>;

template<typename Handle, typename Object>
concept attached_handle_for = attached_handle<Handle> && handle_for<Handle, Object>;

} // namespace allio::detail
