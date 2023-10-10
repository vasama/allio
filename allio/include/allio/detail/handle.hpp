#pragma once

#include <allio/detail/api.hpp>
#include <allio/detail/handle_flags.hpp>
#include <allio/detail/io.hpp>
#include <allio/detail/io_sender.hpp>
#include <allio/detail/lifetime.hpp>
#include <allio/detail/parameters.hpp>
#include <allio/detail/parameters2.hpp>
#include <allio/detail/type_list.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/linear.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/utility.hpp>

#include <cstdint>

namespace allio::detail {

template<typename H, typename M>
class basic_handle;

struct handle_base
{
	using asynchronous_operations = type_list<>;

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


	struct close_t;

protected:
	struct private_t {};


	allio_detail_default_lifetime(handle);

	explicit handle(native_handle_type const& native)
		: m_flags(native.flags)
	{
	}


#if 0
	void set_flags(handle_flags const new_flags) &
	{
		vsm_assert(*this); //PRECONDITION
		vsm_assert(new_flags[flags::not_null]); //PRECONDITION
		m_flags.value = new_flags;
	}
#endif


	[[nodiscard]] static bool check_native_handle(native_handle_type const& native)
	{
		return native.flags[flags::not_null];
	}

	void set_native_handle(native_handle_type const& native) &
	{
		m_flags.value = native.flags;
	}


	template<std::derived_from<handle> H>
	static vsm::result<void> initialize(H& h, auto&& initializer)
	{
		return h._initialize(vsm_forward(initializer));
	}

	template<typename H, typename M, typename O>
	static auto invoke(basic_handle<H, M>& h, io_parameters_t<O> const& args)
	{
		return h._invoke(args);
	}

	template<typename H, typename M, typename O>
	static auto invoke(basic_handle<H, M> const& h, io_parameters_t<O> const& args)
	{
		return h._invoke(args);
	}

	template<typename H>
	struct interface
	{
		void close()
		{
			static_cast<H&>(*this)._close();
		}
	};
};

template<typename M>
struct _basic_multiplexer_handle
{
	class type
	{
		M* m_multiplexer;

	public:
		using multiplexer_type = M;

		type(M& multiplexer)
			: m_multiplexer(&multiplexer)
		{
		}

		template<typename H, typename C, typename S, typename R>
		friend vsm::result<io_result> tag_invoke(
			submit_io_t,
			type const& m,
			H& h,
			C const& c,
			S& s,
			R&& r)
		{
			return submit_io(
				*m.m_multiplexer,
				h,
				c,
				s,
				vsm_forward(r));
		}

		template<typename H, typename C, typename S, typename R>
		friend vsm::result<io_result> tag_invoke(
			notify_io_t,
			type const& m,
			H& h,
			C const& c,
			S& s,
			R&& r,
			io_status const status)
		{
			return notify_io(
				*m.m_multiplexer,
				h,
				c,
				s,
				vsm_forward(r),
				status);
		}

		template<typename H, typename C, typename S>
		friend vsm::result<io_result> tag_invoke(
			cancel_io_t,
			type const& m,
			H& h,
			C const& c,
			S& s)
		{
			return cancel_io(
				*m.m_multiplexer,
				h,
				c,
				s);
		}
	};
};

template<typename M>
using basic_multiplexer_handle = typename _basic_multiplexer_handle<M>::type;

template<typename M>
auto _multiplexer_handle(M const&)
{
	if constexpr (requires { typename M::multiplexer_tag; })
	{
		static_assert(std::is_void_v<typename M::multiplexer_tag>);

		if constexpr (requires { typename M::handle_type; })
		{
			return vsm::tag<typename M::handle_type>();
		}
		else
		{
			return vsm::tag<basic_multiplexer_handle<M>>();
		}
	}
	else
	{
		return vsm::tag<M>();
	}
}

template<typename M>
using multiplexer_handle_t = typename decltype(_multiplexer_handle(vsm_declval(M)))::type;

template<typename M, typename H>
concept multiplexer_for = true;

template<typename H>
class basic_handle<H, void> final
	: public H
	, public H::template interface<basic_handle<H, void>>
{
public:

	void get_multiplexer() const
	{
	}

	void get_connector() const
	{
	}


	template<multiplexer_for<H> M>
	vsm::result<basic_handle<H, multiplexer_handle_t<M>>> with_multiplexer(M&& multiplexer);

private:
	template<vsm::any_cv_of<basic_handle> Self, typename O>
	friend vsm::result<void> tag_invoke(
		blocking_io_t,
		Self& h,
		io_result_ref_t<O> const r,
		io_parameters_t<O> const& args)
	{
		return H::do_blocking_io(
			static_cast<vsm::copy_cv_t<Self, H>&>(h),
			r,
			args);
	}

	template<typename O>
	vsm::result<io_result_t<O>> _invoke(io_parameters_t<O> const& args)
	{
		vsm::result<io_result_t<O>> r(vsm::result_value);
		vsm_try_void(H::do_blocking_io(
			static_cast<H&>(*this),
			r,
			args));
		return r;
	}

	template<typename O>
	vsm::result<io_result_t<O>> _invoke(io_parameters_t<O> const& args) const
	{
		vsm::result<io_result_t<O>> r(vsm::result_value);
		vsm_try_void(H::do_blocking_io(
			static_cast<H const&>(*this),
			r,
			args));
		return r;
	}

	friend handle;

	template<typename, typename>
	friend class basic_handle;
};

template<typename H, typename M>
	requires multiplexer_for<M, H>
class basic_handle<H, M> final
	: public H
	, public H::template interface<basic_handle<H, M>>
{
	using multiplexer_type = typename M::multiplexer_type;
	using connector_type = connector_t<multiplexer_type, H>;

	vsm_no_unique_address M m_multiplexer = {};
	vsm_no_unique_address connector_type m_connector;

public:
	using multiplexer_handle_type = M;


	basic_handle() = default;

	basic_handle(basic_handle&& other) noexcept = default;

	basic_handle& operator=(basic_handle&& other) & noexcept
	{
		if (*this)
		{
			H::close();
		}
		H::operator=(vsm_move(other));
		return *this;
	}

	~basic_handle()
	{
		if (*this)
		{
			H::close();
		}
	}


	[[nodiscard]] M const& get_multiplexer() const
	{
		return m_multiplexer;
	}

	[[nodiscard]] connector_type const& get_connector() const
	{
		return m_connector;
	}

private:
	template<typename O>
	io_sender<basic_handle, O> _invoke(io_parameters_t<O> const& args) const
	{
		return io_sender<basic_handle, O>(*this, args);
	}

	template<vsm::any_cv_of<basic_handle> Self, typename S, typename R>
	friend vsm::result<io_result> tag_invoke(
		submit_io_t,
		Self& h,
		S& s,
		R& r)
	{
		return submit_io(
			static_cast<M const&>(h.m_multiplexer),
			static_cast<vsm::copy_cv_t<Self, H>&>(h),
			static_cast<connector_type const&>(h.m_connector),
			s,
			r);
	}

	template<vsm::any_cv_of<basic_handle> Self, typename S, typename R>
	friend vsm::result<io_result> tag_invoke(
		notify_io_t,
		Self& h,
		S& s,
		R& r,
		io_status const status)
	{
		return notify_io(
			static_cast<M const&>(h.m_multiplexer),
			static_cast<vsm::copy_cv_t<Self, H>&>(h),
			static_cast<connector_type const&>(h.m_connector),
			s,
			r,
			status);
	}

	template<vsm::any_cv_of<basic_handle> Self, typename S>
	friend void tag_invoke(
		cancel_io_t,
		Self& h,
		S& s)
	{
		return cancel_io(
			static_cast<M const&>(h.m_multiplexer),
			static_cast<vsm::copy_cv_t<Self, H>&>(h),
			static_cast<connector_type const&>(h.m_connector),
			s);
	}

	friend handle;

	template<typename, typename>
	friend class basic_handle;
};


#if 0
allio_detail_export
template<typename H>
class basic_blocking_handle;

allio_detail_export
template<typename H, typename M>
class basic_async_handle;


allio_detail_export
template<typename H>
class basic_blocking_handle final
	: public H
	, public H::template sync_interface<basic_blocking_handle<H>>
{
	template<typename M>
	using async_handle_type = basic_async_handle<H, M>;

public:
	basic_blocking_handle() = default;

	basic_blocking_handle(basic_blocking_handle&& other) noexcept = default;

	basic_blocking_handle& operator=(basic_blocking_handle&& other) & noexcept
	{
		if (*this)
		{
			H::close();
		}
		H::operator=(vsm_move(other));
		return *this;
	}

	~basic_blocking_handle()
	{
		if (*this)
		{
			H::close();
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

	static vsm::result<basic_blocking_handle> from_native_handle(typename H::native_handle_type const& native)
	{
		if (!H::check_native_handle(native))
		{
			return vsm::unexpected(error::invalid_argument);
		}

		vsm::result<basic_blocking_handle> r(vsm::result_value);
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

	void _close()
	{
		if (*this)
		{
			H::close();
		}
	}


	template<typename O>
	friend vsm::result<void> tag_invoke(
		blocking_io_t,
		vsm::any_cvref_of<basic_blocking_handle> auto& h,
		io_result_ref_t<O> const result,
		io_parameters_t<O> const& args)
	{
		return H::do_blocking_io(h, result, args);
	}


	friend class handle;

	template<typename, typename>
	friend class basic_async_handle;
};

allio_detail_export
template<typename H, typename M>
class basic_async_handle final
	: public H
	, public H::template sync_interface<basic_async_handle<H, M>>
	, public H::template async_interface<basic_async_handle<H, M>>
{
	using blocking_handle_type = basic_blocking_handle<H>;

public:
	using multiplexer_pointer_type = M;
	using multiplexer_type = multiplexer_t<M>;

private:
	using connector_type = connector_t<multiplexer_type, H>;

	static_assert(std::is_default_constructible_v<M>);
	static_assert(std::is_nothrow_move_assignable_v<M>);

	vsm_no_unique_address M m_multiplexer = {};
	vsm_no_unique_address connector_type m_connector;

public:
	basic_async_handle() = default;

	basic_async_handle(blocking_handle_type&& h)
		: H(static_cast<H&&>(h))
	{
	}

	template<std::convertible_to<M> T = M>
		requires vsm::no_cvref_of<T, basic_async_handle>
	explicit basic_async_handle(T&& multiplexer)
		: m_multiplexer(vsm_forward(multiplexer))
	{
	}

	basic_async_handle(basic_async_handle&& other) noexcept = default;

	basic_async_handle& operator=(basic_async_handle&& other) & noexcept
	{
		if (*this)
		{
			H::close();
		}

		H::operator=(vsm_move(other));
		m_connector = vsm_move(other.m_connector);

		return *this;
	}

	~basic_async_handle()
	{
		if (*this)
		{
			H::close();
		}
	}


	[[nodiscard]] M const& get_multiplexer() const
	{
		return m_multiplexer;
	}

	[[nodiscard]] connector_type const& get_connector() const
	{
		return m_connector;
	}


	template<std::convertible_to<M> T = M>
	vsm::result<void> set_multiplexer(T&& multiplexer) &
	{
		return _set_multiplexer(vsm_forward(multiplexer));
	}

	M release_multiplexer()
	{
		if (*this && m_multiplexer)
		{
			m_multiplexer->detach(static_cast<H const&>(*this), m_connector);
		}

		M multiplexer = vsm_move(m_multiplexer);
		m_multiplexer = {};

		return multiplexer;
	}


	vsm::result<void> set_handle(blocking_handle_type&& handle) &
	{
		if (*this)
		{
			return vsm::unexpected(error::handle_is_not_null);
		}

		return _set_handle(handle);
	}

	vsm::result<blocking_handle_type> release_handle()
	{
		if (!*this)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		vsm::result<blocking_handle_type> r(vsm::result_value);
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

		blocking_handle_type h;
		vsm_try_void(vsm_forward(initializer)(h));
		return _set_handle(h);
	}

	vsm::result<void> _set_multiplexer(M multiplexer)
	{
		if (!multiplexer)
		{
			return vsm::unexpected(error::invalid_argument);
		}

		if (*this)
		{
			vsm_try_void(attach_handle(
				*vsm_as_const(m_multiplexer),
				static_cast<H const&>(*this),
				m_connector));
		}

		m_multiplexer = vsm_move(multiplexer);
		return {};
	}

	vsm::result<void> _set_handle(blocking_handle_type& handle)
	{
		if (m_multiplexer)
		{
			vsm_try_void(attach_handle(*m_multiplexer, static_cast<H const&>(handle), m_connector));
		}

		this->H::operator=(static_cast<H&&>(handle));
		return {};
	}

	void _release_handle(blocking_handle_type& handle)
	{
		if (m_multiplexer)
		{
			detach_handle(*m_multiplexer, static_cast<H const&>(*this), m_connector);
		}

		handle.H::operator=(static_cast<H&&>(*this));
	}

	void _close()
	{
		if (*this)
		{
			if (m_multiplexer)
			{
				m_multiplexer->detach(static_cast<H const&>(*this), m_connector);
			}

			H::close();
		}
	}


	template<typename O>
	friend vsm::result<void> tag_invoke(
		blocking_io_t,
		vsm::any_cvref_of<basic_async_handle> auto& h,
		io_result_ref_t<O> const result,
		io_parameters_t<O> const& args)
	{
		return H::do_blocking_io(h, result, args);
	}

	template<typename S, typename R>
	friend vsm::result<io_result> tag_invoke(submit_io_t, basic_async_handle const& h, S& s, R const r)
	{
		return submit_io(*h.m_multiplexer, static_cast<H const&>(h), vsm_as_const(h.m_connector), s, r);
	}

	template<typename S, typename R>
	friend vsm::result<io_result> tag_invoke(notify_io_t, basic_async_handle const& h, S& s, R const r, io_status const status)
	{
		//TODO: DEBUG
		return tag_invoke(notify_io, *h.m_multiplexer, static_cast<H const&>(h), vsm_as_const(h.m_connector), s, r, status);
		//return notify_io(*h.m_multiplexer, static_cast<H const&>(h), vsm_as_const(h.m_connector), s, r, status);
	}

	template<typename S>
	friend void tag_invoke(cancel_io_t, basic_async_handle const& h, S& s)
	{
		return cancel_io(*h.m_multiplexer, static_cast<H const&>(h), vsm_as_const(h.m_connector), s);
	}


	friend class handle;

	template<typename>
	friend class basic_blocking_handle;
};
#endif

} // namespace allio::detail
