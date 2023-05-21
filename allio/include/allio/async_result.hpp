#pragma once

#include <vsm/concepts.hpp>
#include <vsm/result.hpp>
#include <vsm/tag_ptr.hpp>

namespace allio {
namespace detail {

template<typename T>
struct sync_error_parameter
{
	T&& value;
};

} // namespace detail

class async_error_code
{
	int m_value;
	vsm::tag_ptr<std::error_category const, bool> m_category;

public:
	template<vsm::no_cvref_of<async_error_code> T>
	async_error_code(T&& error)
		requires std::convertible_to<T&&, std::error_code>
		: async_error_code(static_cast<T&&>(error), false)
	{
	}

	template<vsm::no_cvref_of<async_error_code> T>
	async_error_code(detail::sync_error_parameter<T> error)
		requires std::convertible_to<T&&, std::error_code>
		: async_error_code(static_cast<T&&>(error.value), true)
	{
	}

	explicit async_error_code(std::error_code const error, bool const synchronous)
		: m_value(error.value())
		, m_category(&error.category(), synchronous)
	{
	}

	bool is_synchronous() const
	{
		return m_category.tag();
	}

	std::error_code error_code() const
	{
		return std::error_code(m_value, *m_category);
	}
};

template<typename T>
detail::sync_error_parameter<T&&> sync_error(T&& value)
{
	return { static_cast<T&&>(value) };
}

template<typename T>
using async_result = vsm::result<T, async_error_code>;

} // namespace allio
