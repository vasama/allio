#pragma once

#include <allio/detail/opaque_handle.hpp>

#include <vsm/intrusive_ptr.hpp>
#include <vsm/standard.hpp>

namespace allio::detail {

class _opaque_handle : public handle
{
protected:
	using base_type = handle;

private:
	unique_opaque_handle m_opaque_handle;

public:
	struct native_handle_type : base_type::native_handle_type
	{
		detail::opaque_handle* opaque_handle;
	};

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			m_opaque_handle.get(),
		};
	}


	[[nodiscard]] unique_opaque_handle const& get_opaque_handle() const
	{
		return m_opaque_handle.get();
	}


	struct poll_t
	{
		using handle_type = _opaque_handle const;
		using result_type = void;

		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			poll_t
		>
	>;

protected:
	allio_detail_default_lifetime(_opaque_handle);

	explicit _opaque_handle(native_handle_type const& native)
		: base_type(native)
		, m_opaque_handle(native.opaque_handle)
	{
	}


	static bool check_native_handle(native_handle_type const& native)
	{
		return base_type::check_native_handle(native)
			&& native.opaque_handle != nullptr
		;
	}

	void set_native_handle(native_handle_type const& native)
	{
		base_type::set_native_handle(native);
		m_opaque_handle.value = native.opaque_handle;
	}


	void close()
	{
		m_opaque_handle.reset();
	}


	template<typename H>
	struct sync_interface
	{
		vsm::result<void> poll(auto&&... args) const
		{
			return do_blocking_io(
				static_cast<_opaque_handle const&>(*this),
				no_result,
				io_arguments_t<poll_t>()(vsm_forward(args)...));
		}
	};

	template<typename H>
	struct async_interface
	{
		io_sender<H, poll_t> poll_async(auto&&... args) const
		{
			return io_sender<H, poll_t>(
				static_cast<H const&>(*this),
				io_arguments_t<poll_t>()(vsm_forward(args)...));
		}
	};


	static vsm::result<void> do_blocking_io(
		_opaque_handle const& h,
		io_result_ref_t<poll_t> result,
		io_parameters_t<poll_t> const& args);
};

using blocking_opaque_handle = basic_blocking_handle<detail::_opaque_handle>;

template<typename Multiplexer>
using basic_opaque_handle = async_handle<detail::_opaque_handle, Multiplexer>;


template<typename Object>
class shared_opaque_handle
	: public vsm::intrusive_ptr_ref_count
	, opaque_handle
{
	vsm_no_unique_address Object m_object;

public:
	template<std::convertible_to<Object> T = Object>
	explicit shared_opaque_handle(Object&& object)
		: opaque_handle
		{
			.version = allio_abi_version,
			.handle_value = object.handle_value(),
			.handle_information = object.handle_information(),
			.functions = &functions,
		}
		, m_object(object)
	{
	}

private:
	static void _close(opaque_handle* const p_self)
	{
		auto& self = static_cast<shared_opaque_handle&>(*p_self);

		(void)vsm::intrusive_ptr<shared_opaque_handle>::acquire(&self);
	}

	static allio_abi_result _notify(opaque_handle* const p_self, uintptr_t const information)
	{
		auto& self = static_cast<shared_opaque_handle&>(*p_self);

		if constexpr (requires { self.m_object.notify(); })
		{
			self.m_object.notify();
		}
		else
		{
			return allio_abi_result_success;
		}
	}

	static const opaque_handle_functions functions;
};

template<typename Object>
opaque_handle_functions const shared_opaque_handle<Object>::functions =
{
	.close = _close,
	.notify = _notify,
};

template<typename Object>
using shared_opaque_handle_ptr = vsm::intrusive_ptr<shared_opaque_handle<Object>>;

template<typename Object>
shared_opaque_handle_ptr<Object> make_shared_opaque_handle(auto&&... args)
{
	return shared_opaque_handle_ptr<Object>(new shared_opaque_handle<Object>(vsm_forward(args)...));
}

} // namespace allio::detail
