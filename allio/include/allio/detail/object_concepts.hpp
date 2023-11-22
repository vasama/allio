#pragma once

#include <allio/detail/multiplexer.hpp>

#include <concepts>

namespace allio::detail {

struct object_t;

template<typename Object>
concept object = true; //TODO: std::derived_from<Object, object_t>;

template<typename Multiplexer, typename Object>
concept _multiplexer_for = object<Object>; //TODO

template<typename Multiplexer, typename Object>
concept multiplexer_for =
	multiplexer<Multiplexer> &&
	_multiplexer_for<Multiplexer, Object>;

//template<typename Multiplexer, typename Object>
//concept optional_multiplexer_for =
//	std::is_void_v<Multiplexer> ||
//	multiplexer_for<Multiplexer, Object>;

template<typename MultiplexerHandle, typename Object>
concept multiplexer_handle_for =
	multiplexer_handle<MultiplexerHandle> &&
	_multiplexer_for<typename MultiplexerHandle::multiplexer_type, Object>;

template<typename MultiplexerHandle, typename Object>
concept optional_multiplexer_handle_for =
	std::is_void_v<MultiplexerHandle> ||
	multiplexer_handle_for<MultiplexerHandle, Object>;

} // namespace allio::detail
