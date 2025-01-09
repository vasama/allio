#pragma once

#include <vsm/utility.hpp>

#include <future>
#include <type_traits>

namespace allio::test {

template<typename T>
class future
{
	std::future<T> m_future;

public:
	explicit future(std::future<T> future)
		: m_future(vsm_move(future))
	{
	}

	future(future&&) = default;
	future& operator=(future&&) = default;

	~future()
	{
		try
		{
			(void)m_future.get();
		}
		catch (...)
		{
		}
	}

	[[nodiscard]] T get()
	{
		return m_future.get();
	}
};

template<typename Callable>
static future<std::decay_t<std::invoke_result_t<Callable>>> spawn(Callable&& callable)
{
	return future<std::decay_t<std::invoke_result_t<Callable>>>(
		std::async(std::launch::async, vsm_forward(callable)));
}

} // namespace allio::test
