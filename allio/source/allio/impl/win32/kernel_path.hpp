#pragma once

#include <allio/input_path_view.hpp>
#include <allio/win32/detail/win32_fwd.hpp>

#include <allio/impl/win32/peb.hpp>

#include <vsm/result.hpp>

#include <memory>

namespace allio {
namespace detail::kernel_path_impl {

template<typename Context>
class basic_kernel_path_converter;

template<typename Lock>
class basic_kernel_path_storage
{
	static constexpr size_t small_path_size = 272;
	static constexpr size_t max_root_size = 8;

	std::unique_ptr<wchar_t[]> m_dynamic;
	wchar_t m_static[small_path_size];
	wchar_t m_root[max_root_size];

	Lock m_lock;

public:
	basic_kernel_path_storage() = default;
	basic_kernel_path_storage(basic_kernel_path_storage const&) = delete;
	basic_kernel_path_storage& operator=(basic_kernel_path_storage const&) = delete;

private:
	template<typename Context>
	friend class basic_kernel_path_converter;
};

template<typename Handle>
struct basic_kernel_path_parameters
{
	Handle handle = {};
	input_path_view path;

	// Use full path instead of handle and include a null terminator.
	bool win32_api_form = false;
};

template<typename Handle>
struct basic_kernel_path
{
	Handle handle;
	std::wstring_view path;
};

} // namespace detail::kernel_path_impl
namespace win32 {

using kernel_path_storage = detail::kernel_path_impl::basic_kernel_path_storage<unique_peb_lock>;
using kernel_path_parameters = detail::kernel_path_impl::basic_kernel_path_parameters<detail::HANDLE>;
using kernel_path = detail::kernel_path_impl::basic_kernel_path<detail::HANDLE>;

vsm::result<kernel_path> make_kernel_path(kernel_path_storage& storage, kernel_path_parameters const& args);

} // namespace win32
} // namespace allio
