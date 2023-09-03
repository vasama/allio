#pragma once

#include <allio/detail/api.hpp>
#include <allio/detail/async_io.hpp>
#include <allio/detail/handle_flags.hpp>
#include <allio/detail/parameters.hpp>
#include <allio/detail/type_list.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/linear.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <cstdint>

namespace allio::detail {

struct handle_base
{
	using async_operations = type_list<>;

	struct flags : handle_flags
	{
		flags() = delete;
		static constexpr uint32_t allio_detail_handle_flags_flag_count = 0;
		static constexpr uint32_t allio_detail_handle_flags_flag_limit = 16;
	};

	struct impl_type
	{
		struct flags : handle_flags
		{
			flags() = delete;
			static constexpr uint32_t allio_detail_handle_flags_flag_count = 16;
			static constexpr uint32_t allio_detail_handle_flags_flag_limit = 32;
		};
	};
};


#define allio_detail_default_lifetime(handle) \
	handle() = default; \
	handle(handle&&) = default; \
	handle& operator=(handle&&) = default; \
	~handle() = default \

allio_detail_export
class handle : public handle_base
{
protected:
	using base_type = handle_base;

private:
	vsm::linear<handle_flags> m_flags;

public:
	allio_handle_flags
	(
		not_null,
	);


	struct native_handle_type
	{
		handle_flags flags;
	};

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		vsm_assert(*this); //PRECONDITION
		return
		{
			m_flags.value,
		};
	}


	[[nodiscard]] handle_flags get_flags() const
	{
		vsm_assert(*this); //PRECONDITION
		return m_flags.value;
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_flags.value[flags::not_null];
	}


	#define allio_handle_create_parameters(type, data, ...) \
		type(allio::handle, create_parameters) \
		data(bool, multiplexable, false) \

	allio_interface_parameters(allio_handle_create_parameters);

	struct close_t;
	#define allio_handle_close_parameters(type, data, ...) \
		type(::allio::detail::handle, close_parameters) \
		data(::allio::error_handler*, error_handler, nullptr) \
	
	allio_interface_parameters(allio_handle_close_parameters);

protected:
	struct private_t {};


	allio_detail_default_lifetime(handle);

	explicit handle(native_handle_type const& native)
		: m_flags(native.flags)
	{
	}


	[[nodiscard]] static bool check_native_handle(native_handle_type const& native)
	{
		return native.flags[flags::not_null];
	}

	void set_native_handle(native_handle_type const& native)
	{
		m_flags.value = native.flags;
	}


	template<std::derived_from<handle> H>
	static vsm::result<void> initialize(H& h, auto&& initializer)
	{
		return h._initialize(vsm_forward(initializer));
	}


	template<typename H>
	struct sync_interface
	{
		template<parameters<close_parameters> P = close_parameters::interface>
		void close(P const& args = {})
		{
			static_cast<H&>(*this)._close(args);
		}
	};

	template<typename M, typename H>
	struct async_interface
	{
		//template<parameters<close_parameters> P = close_parameters::interface>
		//basic_sender<M, H, close_t> close_async(P const& args = {});
	};
};


allio_detail_export
template<typename H>
class basic_handle;

allio_detail_export
template<typename H, typename M>
class basic_async_handle;


allio_detail_export
template<typename H>
class basic_handle final
	: public H
	, public H::template sync_interface<basic_handle<H>>
{
	template<typename M>
	using async_handle_type = basic_async_handle<H, M>;

public:
	using base_type = H;


	basic_handle() = default;

	basic_handle(basic_handle&& other) noexcept = default;

	basic_handle& operator=(basic_handle&& other) & noexcept
	{
		if (*this)
		{
			H::close(nullptr);
		}
		H::operator=(vsm_move(other));
		return *this;
	}

	~basic_handle()
	{
		if (*this)
		{
			H::close(nullptr);
		}
	}


	typename H::native_handle_type get_native_handle() const
	{
		vsm_assert(*this); //PRECONDITION
		return H::get_native_handle();
	}

	vsm::result<typename H::native_handle_type> release_native_handle()
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		return H::release_native_handle();
	}

	vsm::result<void> set_native_handle(typename H::native_handle_type const& native) &
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		if (!H::check_native_handle(native))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		H::set_native_handle(native);

		return {};
	}

	static vsm::result<basic_handle> from_native_handle(typename H::native_handle_type const& native)
	{
		if (!H::check_native_handle(native))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		vsm::result<basic_handle> r(vsm::result_value);
		r->H::set_native_handle(native);
		return r;
	}


	template<typename M>
	vsm::result<async_handle_type<std::remove_cvref_t<M>>> with_multiplexer(M&& multiplexer) &&
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}
		
		if (!multiplexer)
		{
			return vsm::unexpected(error::multiplexer_is_null);
		}
		
		vsm::result<async_handle_type<std::remove_cvref_t<M>>> r(vsm_forward(multiplexer));
		vsm_try_void(r->_set_handle(*this));
		return r;
	}

private:
	vsm::result<void> _initialize(auto&& initializer)
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		return vsm_forward(initializer)(*this);
	}

	void _close(handle::close_parameters const& args)
	{
		if (*this)
		{
			return H::close(args.error_handler);
		}
	}


	friend class handle;

	template<typename, typename>
	friend class basic_async_handle;
};


template<typename M>
using multiplexer_t = std::remove_cvref_t<decltype(*std::declval<M const&>())>;

allio_detail_export
template<typename H, typename M>
class basic_async_handle final
	: public H
	, public H::template sync_interface<basic_async_handle<H, M>>
	, public H::template async_interface<multiplexer_t<M>, basic_async_handle<H, M>>
{
	using handle_type = basic_handle<H>;

	using multiplexer_type = multiplexer_t<M>;
	using context_type = async_handle_storage<multiplexer_type, H>;

	static_assert(std::is_default_constructible_v<M>);
	static_assert(std::is_nothrow_move_assignable_v<M>);

	vsm_no_unique_address M m_multiplexer = {};
	vsm_no_unique_address context_type m_context;

public:
	using base_type = H;


	basic_async_handle() = default;

	basic_async_handle(handle_type&& h)
		: H(static_cast<H&&>(h))
	{
	}

	template<std::convertible_to<M> T = M>
		requires no_cvref_of<T, basic_async_handle>
	explicit basic_async_handle(T&& multiplexer)
		: m_multiplexer(vsm_forward(multiplexer))
	{
	}

	basic_async_handle(basic_async_handle&& other) noexcept = default;

	basic_async_handle& operator=(basic_async_handle&& other) & noexcept
	{
		if (*this)
		{
			H::close(nullptr);
		}

		H::operator=(vsm_move(other));
		m_context = vsm_move(other.m_context);

		return *this;
	}

	~basic_async_handle()
	{
		if (*this)
		{
			H::close(nullptr);
		}
	}


	[[nodiscard]] M const& get_multiplexer() const
	{
		return m_multiplexer;
	}

	template<std::convertible_to<M> T = M>
	vsm::result<void> set_multiplexer(T&& multiplexer) &
	{
		return _set_multiplexer(vsm_forward(multiplexer));
	}

	M release_multiplexer(error_handler* const error_handler = nullptr)
	{
		if (*this && m_multiplexer)
		{
			m_multiplexer->detach(static_cast<H const&>(*this), m_context, error_handler);
		}

		M multiplexer = vsm_move(m_multiplexer);
		m_multiplexer = {};

		return multiplexer;
	}


	vsm::result<void> set_handle(handle_type&& handle) &
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		return _set_handle(handle);
	}

	vsm::result<handle_type> release_handle()
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		vsm::result<handle_type> r(vsm::result_value);
		vsm_try_void(_release_handle(*r));
		return r;
	}


private:
	vsm::result<void> _initialize(auto&& initializer)
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		handle_type h;
		vsm_try_void(vsm_forward(initializer)(h));
		return _set_handle(h);
	}

	vsm::result<void> _set_multiplexer(M&& multiplexer)
	{
		if (!multiplexer)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		if (*this)
		{
			vsm_try_void(multiplexer->attach(static_cast<H const&>(*this), m_context));
		}

		m_multiplexer = vsm_move(multiplexer);
		return {};
	}

	vsm::result<void> _set_handle(handle_type& handle)
	{
		if (m_multiplexer)
		{
			vsm_try_void(m_multiplexer->attach(static_cast<H const&>(handle), m_context));
		}

		this->H::operator=(static_cast<H&&>(handle));
		return {};
	}

	void _release_handle(handle_type& handle)
	{
		if (m_multiplexer)
		{
			m_multiplexer->detach(static_cast<H const&>(*this), m_context, nullptr);
		}

		handle.H::operator=(static_cast<H&&>(*this));
		return {};
	}

	void _close(error_handler* const error_handler)
	{
		if (*this)
		{
			if (m_multiplexer)
			{
				m_multiplexer->detach(static_cast<H const&>(*this), m_context, error_handler);
			}

			H::close(error_handler);
		}
	}


	friend class handle;

	template<typename>
	friend class basic_handle;
};

} // namespace allio::detail
