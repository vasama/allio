#pragma once

#include <allio/detail/io.hpp>
#include <allio/detail/type_list.hpp>

#include <vsm/math.hpp>
#include <vsm/utility.hpp>

#include <memory>
#include <new>

namespace allio {
namespace detail::_any_multiplexer {

template<size_t MinStorageSize>
class any_object
{
	static constexpr size_t storage_size = vsm::round_up_to_power_of_two(
		MinStorageSize > 1 ? MinStorageSize : 1,
		sizeof(void*));

	alignas(std::max_align_t) std::byte m_storage[storage_size];
	void(*m_move_and_or_destroy)(void* object, void* new_storage);

	template<typename T>
	static constexpr bool can_store_locally =
		sizeof(T) <= storage_size &&
		std::is_nothrow_move_constructible_v<T>;

	template<typename T, bool = can_store_locally<T>>
	struct wrapper;

	template<typename T>
	struct wrapper<T, 1>
	{
		T object;

		template<typename... Args>
		explicit wrapper(Args&&... args)
			: object(vsm_forward(args)...)
		{
		}

		T& get()
		{
			return object;
		}

		T const& get() const
		{
			return object;
		}

		static void _move_and_or_destroy(void* const object, void* const new_storage) noexcept
		{
			auto const source = std::launder(static_cast<wrapper*>(object));
			if (new_storage != nullptr)
			{
				(void)new (new_storage) wrapper(vsm_move(source->object));
			}
			source->~wrapper();
		}
	};

	template<typename T>
	struct wrapper<T, 0>
	{
		T* object;

		T& get()
		{
			return *object;
		}

		T const& get() const
		{
			return *object;
		}

		static void _move_and_or_destroy(void* const object, void* const new_storage) noexcept
		{
			auto const source = std::launder(static_cast<wrapper*>(object));
			if (new_storage != nullptr)
			{
				(void)new (new_storage) wrapper{ source->object };
			}
			else
			{
				delete source->object;
			}
		}
	};

public:
	template<typename T, typename... Args>
	explicit any_object(std::in_place_type_t<T>, Args&&... args)
		requires can_store_locally<T> && std::constructible_from<T, Args&&...>
	{
		(void)new (m_storage) wrapper<T>(vsm_forward(args)...);
	}

	any_object(any_object&& other)
	{
		m_move_and_or_destroy = other.m_move_and_or_destroy;
		if (other.m_move_and_or_destroy != nullptr)
		{
			other.m_move_and_or_destroy(other.m_storage, m_storage);
			other.m_move_and_or_destroy = nullptr;
		}
	}

	any_object& operator=(any_object&& other) &
	{
		if (m_move_and_or_destroy != nullptr)
		{
			m_move_and_or_destroy(m_storage, nullptr);
			m_move_and_or_destroy = nullptr;
		}

		m_move_and_or_destroy = other.m_move_and_or_destroy;
		if (other.m_move_and_or_destroy != nullptr)
		{
			other.m_move_and_or_destroy(other.m_storage, m_storage);
			other.m_move_and_or_destroy = nullptr;
		}
	}

	~any_object()
	{
		if (m_move_and_or_destroy != nullptr)
		{
			m_move_and_or_destroy(m_storage, nullptr);
		}
	}


	template<typename T, typename... Args>
	[[nodiscard]] vsm::result<T*> try_emplace(Args&&... args) &
	{
		if (m_move_and_or_destroy != nullptr)
		{
			m_move_and_or_destroy(m_storage, nullptr);
			m_move_and_or_destroy = nullptr;
		}

		if constexpr (can_store_locally<T>)
		{
			auto const w = new (m_storage) wrapper<T>(vsm_forward(args)...);
			m_move_and_or_destroy = wrapper<T>::_move_and_or_destroy;
			return w->object;
		}
		else
		{
			auto const object = new (std::nothrow) T(vsm_forward(args)...);
			if (object == nullptr)
			{
				return vsm::unexpected(error::not_enough_memory);
			}
			auto const w = new (m_storage) wrapper<T>{ object };
			m_move_and_or_destroy = wrapper<T>::_move_and_or_destroy;
			return w->object;
		}
	}

	template<typename T>
	[[nodiscard]] bool has() const
	{
		return m_move_and_or_destroy == wrapper<T>::_move_and_or_destroy;
	}

	template<typename T>
	[[nodiscard]] T& get()
	{
		vsm_assert(has<T>());
		return std::launder(reinterpret_cast<wrapper<T>*>(m_storage))->get();
	}

	template<typename T>
	[[nodiscard]] T const& get() const
	{
		vsm_assert(has<T>());
		return std::launder(reinterpret_cast<wrapper<T> const*>(m_storage))->get();
	}
};

template<typename H, typename O>
struct operation : any_object<sizeof(io_parameters_t<O>)>
{
	operation(io_parameters_t<O> const& args)
		: any_object<sizeof(io_parameters_t<O>)>(args)
	{
	}
};

template<typename H>
struct connector : any_object<0>
{
};

template<typename H, typename O>
struct operation_vtable
{
	using C = connector<H>;
	using S = operation<H, O>;

	using handle_type = typename O::handle_type;

	io_result2(*submit)(void* m, handle_type& h, C const& c, S& s, void* r, io_status status);
	io_result2(*notify)(void* m, handle_type& h, C const& c, S& s, void* r);
	void                  (*cancel)(void* m, handle_type& h, C const& c, S& s);

	template<typename M>
	explicit constexpr operation_vtable(M*)
		: submit(_submit<M>)
		, notify(_notify<M>)
		, cancel(_cancel<M>)
	{
	}

	template<typename M>
	static io_result2 _submit(
		void* const m,
		handle_type& h,
		C const& c,
		S& s,
		io_result_ref_t<O> const r)
	{
		using MC = connector_t<M, H>;
		using MS = operation_t<M, H, O>;

		if (!c.template has<MS>())
		{
			auto const args = c.template get<io_parameters_t<O>>();
			vsm_try_discard(c.template try_emplace<MS>(args));
		}

		return submit_io(
			*static_cast<M const*>(m),
			h,
			c.get<MC>(),
			s.get<MS>(),
			r);
	}

	template<typename M>
	static io_result2 _notify(
		void* const m,
		handle_type& h,
		C const& c,
		S& s,
		io_result_ref_t<O> const r,
		io_status const status)
	{
		using MC = connector_t<M, H>;
		using MS = operation_t<M, H, O>;

		return notify_io(
			*static_cast<M const*>(m),
			h,
			c.get<MC>(),
			s.get<MS>(),
			r,
			status);
	}

	template<typename M>
	static void _cancel(void* const m, handle_type& h, C const& c, S& s)
	{
		using MC = connector_t<M, H>;
		using MS = operation_t<M, H, O>;

		return cancel_io(
			*static_cast<M const*>(m),
			h,
			c.get<MC>(),
			s.get<MS>());
	}
};

template<typename H>
struct connector_vtable
{
	using C = connector<H>;

	vsm::result<void>(*attach)(void* m, H const& h, C& c);
	vsm::result<void>(*detach)(void* m, H const& h, C& c);

	template<typename M>
	explicit constexpr connector_vtable(M*)
		: attach(_attach<M>)
		, detach(_detach<M>)
	{
	}

	template<typename M>
	static vsm::result<void> _attach(void* const m, H const& h, C& c)
	{
		using MC = connector_t<M, H>;
		
		return attach_handle(
			*static_cast<M const*>(m),
			h,
			c.get<MC>());
	}

	template<typename M>
	static vsm::result<void> _detach(void* const m, H const& h, C& c)
	{
		using MC = connector_t<M, H>;
		
		return detach_handle(
			*static_cast<M const*>(m),
			h,
			c.get<MC>());
	}
};

template<typename H, typename O = typename H::async_operations>
struct handle_vtable;

template<typename H, typename... O>
struct handle_vtable<H, type_list<O...>>
	: connector_vtable<H>
	, operation_vtable<H, O>...
{
	template<typename M>
	explicit constexpr handle_vtable(M* m)
		: connector_vtable<H>(m)
		, operation_vtable<H, O>(m)...
	{
	}
};

template<typename... H>
struct vtable : handle_vtable<H>...
{
	template<typename M>
	explicit constexpr vtable(M* m)
		: handle_vtable<H>(m)...
	{
	}
};

template<typename M, typename... H>
inline constexpr vtable<H...> _vtable(static_cast<M*>(0));

struct any_multiplexer_tag {};

template<typename... Handles>
struct handle
{
	struct type
	{
		void* m_multiplexer;
		vtable<Handles...> const* m_vtable;
	
	public:
		using multiplexer_type
		{

		};

		template<typename Multiplexer>
		type(Multiplexer& multiplexer)
			: m_multiplexer(&multiplexer)
			, m_vtable(&_vtable<Multiplexer>)
		{
		}
	
		explicit operator bool() const
		{
			return m_multiplexer != nullptr;
		}
	
	private:
		template<typename H>
		friend vsm::result<void> tag_invoke(
			attach_handle_t,
			type const& m,
			H const& h,
			connector<H>& c)
		{
			return static_cast<connector_vtable<H> const*>(m_vtable)->attach(
				m.m_multiplexer,
				h,
				c);
		}
		
		template<typename H>
		friend vsm::result<void> tag_invoke(
			detach_handle_t,
			type const& m,
			H const& h,
			connector<H>& c)
		{
			return static_cast<connector_vtable<H> const*>(m_vtable)->detach(
				m.m_multiplexer,
				h,
				c);
		}
	
		template<typename H, typename O>
		friend io_result2 tag_invoke(
			submit_io_t,
			type const& m,
			H&& h,
			connector<H> const& c,
			operation<H, O>& s,
			io_result_ref_t<O> const r)
		{
			return static_cast<operation_vtable<H, O> const*>(m_vtable)->submit(
				m.m_multiplexer,
				h,
				c,
				s,
				r);
		}
	
		template<typename H, typename O>
		friend io_result2 tag_invoke(
			notify_io_t,
			type const& m,
			H&& h,
			connector<H> const& c,
			operation<H, O>& s,
			io_result_ref_t<O> const r,
			io_status const status)
		{
			return static_cast<operation_vtable<H, O> const*>(m_vtable)->notify(
				m.m_multiplexer,
				h,
				c,
				s,
				r,
				status);
		}
	
		template<typename H, typename O>
		friend void tag_invoke(
			cancel_io_t,
			type const& m,
			H&& h,
			connector<H> const& c,
			operation<H, O>& s)
		{
			return static_cast<operation_vtable<H, O> const*>(m_vtable)->cancel(
				m.m_multiplexer,
				h,
				c,
				s);
		}
	};
};

} // namespace detail::_any_multiplexer

template<typename... Handles>
using any_multiplexer_handle = typename detail::_any_multiplexer::handle<Handles...>::type;

} // namespace allio
