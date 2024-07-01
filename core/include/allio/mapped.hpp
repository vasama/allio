#if 0
#pragma once

#include <allio/detail/object_concepts.hpp>

#include <vsm/concepts.hpp>
#include <vsm/utility.hpp>

namespace allio {

template<typename T, typename Handle>
class basic_mapped
{
	Handle m_handle;

public:
	explicit basic_mapped(vsm::any_cvref_of<Handle> auto&& handle)
		: m_handle(vsm_forward(handle))
	{
	}

	[[nodiscard]] T* data() const
	{
		return static_cast<T*>(m_handle.base());
	}

	[[nodiscard]] size_t size() const
	{
		return m_handle.size() / sizeof(T);
	}

	[[nodiscard]] bool empty() const
	{
		return m_handle.size() == 0;
	}

	[[nodiscard]] T* begin() const
	{
		return static_cast<T*>(m_handle.base());
	}

	[[nodiscard]] T* end() const
	{
		return static_cast<T*>(m_handle.base()) + m_handle.size() / sizeof(T);
	}
};

template<typename T, typename Handle>
auto make_mapped(Handle&& handle)
{
}

} // namespace allio
#endif
