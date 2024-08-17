#pragma once

#include <allio/detail/object_concepts.hpp>

#include <vsm/concepts.hpp>
#include <vsm/utility.hpp>

#include <span>

namespace allio {
namespace detail {

struct map_t;

template<detail::handle_for<map_t> Handle>
struct mapped_base
{
	Handle m_handle;
};

} // namespace detail

template<vsm::non_ref T, detail::handle_for<map_t> Handle>
class basic_mapped
	: detail::mapped_base<Handle>
	, std::span<T>
{
	using base_type = detail::mapped_base<Handle>;
	using span_type = std::span<T>;
	using void_type = vsm::copy_cv_t<T, void>;

public:
	using element_type                  = typename span_type::element_type;
	using value_type                    = typename span_type::value_type;
	using size_type                     = typename span_type::size_type;
	using difference_type               = typename span_type::difference_type;
	using pointer                       = typename span_type::pointer;
	using const_pointer                 = typename span_type::const_pointer;
	using reference                     = typename span_type::reference;
	using const_reference               = typename span_type::const_reference;
	using iterator                      = typename span_type::iterator;
	using const_iterator                = typename span_type::const_iterator;
	using reverse_iterator              = typename span_type::reverse_iterator;
	using const_reverse_iterator        = typename span_type::const_reverse_iterator;

	basic_mapped() = default;

	explicit basic_mapped(vsm::any_cvref_of<Handle> auto&& handle)
		: base_type(vsm_forward(handle))
		, span_type(
			static_cast<T*>(base_type::m_handle.base()),
			base_type::m_handle.size() / sizeof(T))
	{
	}

	explicit basic_mapped(vsm::any_cvref_of<Handle> auto&& handle, size_t const size)
		: base_type(vsm_forward(handle))
		, span_type(
			static_cast<T*>(base_type::m_handle.base()),
			size)
	{
		vsm_assert(size * sizeof(T) <= base_type::m_handle.size());
	}

	explicit basic_mapped(vsm::any_cvref_of<Handle> auto&& handle, std::span<T> const span)
		: base_type(vsm_forward(handle))
		, span_type(span)
	{
		auto const beg = reinterpret_cast<std::byte const*>(base_type::m_handle.base());
		auto const end = beg + base_type::m_handle.size();

		vsm_assert(!std::less()(beg, span.data()));
		vsm_assert(std::less()(span.data() + span.size(), end));
	}

	template<typename Self>
	[[nodiscard]] vsm::copy_cvref_t<Self, Handle>&& get_handle(this Self&& self)
	{
		return vsm_forward(self).base_type::m_handle;
	}

	using span_type::empty;
	using span_type::size;
	using span_type::size_bytes;

	using span_type::front;
	using span_type::back;
#if __cpp_lib_span >= 202311L
	using span_type::at;
#endif
	using span_type::data;
	using span_type::operator[];

	using span_type::begin;
	using span_type::rbegin;
	using span_type::cbegin;
	using span_type::crbegin;
	using span_type::end;
	using span_type::rend;
	using span_type::cend;
	using span_type::crend;

	using span_type::first;
	using span_type::last;
	using span_type::subspan;

	[[nodiscard]] basic_mapped submapped(
		size_t const offset,
		size_t const count = std::dynamic_extent) &&
	{
		return basic_mapped(
			vsm_move(base_type::m_handle),
			span_type::subspan(offset, count));
	}

	[[nodiscard]] basic_mapped submapped(
		size_t const offset,
		size_t const count = std::dynamic_extent) const
		requires std::is_copy_constructible_v<Handle>
	{
		return basic_mapped(
			base_type::m_handle,
			span_type::subspan(offset, count));
	}
};

} // namespace allio
