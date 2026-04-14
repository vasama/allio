#pragma once

#include <allio/any_path.hpp>
#include <allio/detail/basic_storage.hpp>
#include <allio/detail/unique_handle.hpp>
#include <allio/impl/win32/peb.hpp>
#include <allio/win32/detail/win32_fwd.hpp>

#include <vsm/result.hpp>

#include <memory>

namespace allio {
namespace detail::kernel_path_impl {

template<typename Handle>
struct basic_kernel_path
{
	Handle handle;
	std::wstring_view path;
};

template<typename Lock, typename Handle>
class basic_kernel_path_handle
{
	Lock m_peb_lock;
	Handle m_handle = {};

public:
	basic_kernel_path_handle() = default;

	basic_kernel_path_handle(basic_kernel_path_handle const&) = delete;
	basic_kernel_path_handle& operator=(basic_kernel_path_handle const&) = delete;

	void set(Handle const handle) &
	{
		if (m_peb_lock.owns_lock())
		{
			m_peb_lock.unlock();
		}

		m_handle = handle;
	}

	void set(Handle const handle, Lock&& lock) &
	{
		m_peb_lock = vsm_move(lock);
		m_handle = handle;
	}

	[[nodiscard]] Handle get() const
	{
		return m_handle;
	}

	void close()
	{
		m_handle = {};

		if (m_peb_lock.owns_lock())
		{
			m_peb_lock.unlock();
		}
	}
};

} // namespace detail::kernel_path_impl
namespace win32 {

using kernel_path_storage_base = detail::basic_storage_base<wchar_t>;
using kernel_path_storage = detail::basic_storage<wchar_t, 272>;

using kernel_path_handle_base = detail::kernel_path_impl::basic_kernel_path_handle<
	unique_peb_lock,
	detail::HANDLE>;

struct kernel_path_handle : kernel_path_handle_base
{
	[[nodiscard]] vsm::result<detail::unique_handle> duplicate() const;

	[[nodiscard]] vsm::result<detail::unique_handle> duplicate_and_close()
	{
		vsm::result<detail::unique_handle> r = duplicate();

		if (r)
		{
			close();
		}

		return r;
	}
};

vsm::result<std::wstring_view> make_kernel_path(
	kernel_path_storage_base& storage,
	detail::HANDLE base,
	any_path_view path,
	kernel_path_handle* out_handle);

} // namespace win32
} // namespace allio
