#pragma once

#include <allio/detail/blocking_traits.hpp>
#include <allio/detail/senders/senders.hpp>

namespace allio::detail {

class sender_traits : public blocking_traits
{
public:
	using attached_traits = sender_traits;

	template<object Object, optional_multiplexer_handle_for<Object> MultiplexerHandle = void>
	using handle = basic_facade<basic_handle<Object, MultiplexerHandle>, sender_traits>;

	using blocking_traits::observe;

	template<observer Operation, attached_handle Handle, std::derived_from<sender_traits> Traits>
	[[nodiscard]] static ex::sender auto observe(
		basic_facade<Handle, Traits> const& h,
		io_parameters_t<typename Handle::object_type, Operation> const& a)
	{
		return io_sender<basic_facade<Handle, Traits>, Operation>(h, a);
	}

private:
	template<object Object>
	struct handle_template
	{
		//TODO: Can this constraint be re-enabled?
		//template<multiplexer_handle_for<Object> MultiplexerHandle>
		template<typename MultiplexerHandle>
		using type = handle<Object, MultiplexerHandle>;
	};

public:
	template<object Object, producer Operation>
	[[nodiscard]] static ex::sender auto produce(io_parameters_t<Object, Operation> const& a)
	{
		return io_handle_sender<Object, Operation, handle_template<Object>::template type>(a);
	}

	template<typename PreviousResult, typename Function>
	static auto transform(PreviousResult&& previous_sender, Function&& function)
	{
		return
			vsm_forward(previous_sender) |
			ex::then([function = vsm_forward(function)](auto&& previous_result) mutable
			{
				return vsm_move(function)(vsm_forward(previous_result));
			});
	}

	//TODO: Implement sender_traits::and_then
	template<typename PreviousResult, typename Function>
	static auto and_then(PreviousResult&& previous_sender, Function&& function);
};

} // namespace allio::detail
