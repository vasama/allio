#pragma once

#include <vsm/tag_ptr.hpp>

namespace allio {

template<typename T>
class reference
{
	vsm::tag_ptr<T, bool> m_ptr;

public:
	reference(T& object)
		: m_ptr(&object, false)
	{
	}

	reference(T&& object)
		: m_ptr(&object, true)
	{
	}
};

} // namespace allio
