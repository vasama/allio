#pragma once

#include <allio/encoding.hpp>

namespace allio {
namespace detail {

template<typename Encoding>
class path_encoding_base
{
public:
	using encoding = Encoding;
};

template<>
class path_encoding_base<void>
{
};

} // namespace detail

template<typename Char, typename String, typename Encoding = void>
class basic_path_adaptor;

} // namespace allio
