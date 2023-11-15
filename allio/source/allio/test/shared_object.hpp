#pragma once

#include <vsm/utility.hpp>

#include <memory>

namespace allio::test {

template<typename T>
class shared_object
{
	std::shared_ptr<T> m_ptr;

public:
	template<std::convertible_to<T> U = T>
	shared_object(U&& value)
		: m_ptr(std::make_shared<T>(vsm_forward(value)))
	{
	}

	explicit shared_object(std::shared_ptr<T> ptr)
		: m_ptr(vsm_move(ptr))
	{
	}

	T const& get() const
	{
		return *m_ptr;
	}

	operator T const&() const
	{
		return *m_ptr;
	}

	T const* operator->() const
	{
		return m_ptr.get();
	}
};

} // namespace allio::test
