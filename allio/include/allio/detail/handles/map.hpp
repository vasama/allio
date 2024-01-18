#pragma once

#include <allio/detail/handles/section.hpp>

#include <optional>

namespace allio::detail {

enum class map_options : uint8_t
{
	backing_section                     = 1 << 0,
	initial_commit                      = 1 << 1,
	at_fixed_address                    = 1 << 2,
};
vsm_flag_enum(map_options);

struct initial_commit_t : explicit_argument<initial_commit_t, bool> {};
inline constexpr explicit_parameter<initial_commit_t> initial_commit = {};

struct at_fixed_address_t : explicit_argument<at_fixed_address_t, uintptr_t> {};
inline constexpr explicit_parameter<at_fixed_address_t> at_fixed_address = {};

namespace map_io {

struct map_memory_t
{
	using operation_concept = producer_t;

	struct params_type
	{
		map_options options = {};
		detail::protection protection = {};
		detail::page_level page_level = {};
		native_handle<section_t> const* section = nullptr;
		fs_size section_offset = 0;
		size_t size = 0;
		uintptr_t address = 0;

		friend void tag_invoke(set_argument_t, params_type& args, initial_commit_t const value)
		{
			if (!value.value)
			{
				args.options &= ~map_options::initial_commit;
			}
		}

		friend void tag_invoke(set_argument_t, params_type& args, detail::protection const value)
		{
			args.protection = value;
		}

		friend void tag_invoke(set_argument_t, params_type& args, detail::page_level const value)
		{
			args.page_level = value;
		}

		friend void tag_invoke(set_argument_t, params_type& args, at_fixed_address_t const value)
		{
			args.options |= map_options::at_fixed_address;
			args.address = value.value;
		}
	};

	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<map_memory_t>,
		native_handle<Object>& h,
		io_parameters_t<Object, map_memory_t> const& a)
		requires requires { Object::map_memory(h, a); }
	{
		return Object::map_memory(h, a);
	}
};

struct commit_t
{
	using operation_concept = void;

	struct params_type
	{
		void* base = nullptr;
		size_t size = 0;
		detail::protection protection = {};

		friend void tag_invoke(set_argument_t, params_type& args, detail::protection const value)
		{
			args.protection = value;
		}
	};

	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<commit_t>,
		native_handle<Object> const& h,
		io_parameters_t<Object, commit_t> const& a)
		requires requires { Object::commit(h, a); }
	{
		return Object::commit(h, a);
	}
};

struct decommit_t
{
	using operation_concept = void;

	struct params_type
	{
		void* base = nullptr;
		size_t size = 0;
	};

	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<decommit_t>,
		native_handle<Object> const& h,
		io_parameters_t<Object, decommit_t> const& a)
		requires requires { Object::decommit(h, a); }
	{
		return Object::decommit(h, a);
	}
};

} // namespace map_io



template<object Object>
struct shared_native_handle
{
	vsm::atomic<size_t> ref_count;
	native_handle<Object> h;

	void acquire()
	{
		ref_count.fetch_add(1, std::memory_order_relaxed);
	}

	void release()
	{
		if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			unrecoverable(blocking_io<close_t>(
				h,
				no_parameters_t()));

			memory_release(
				this,
				sizeof(shared_native_handle),
				alignof(shared_native_handle),
				/* automatic: */ false);
		}
	}
};

using shared_section_handle = shared_native_handle<section_t>;

struct map_t : object_t
{
	using base_type = object_t;

	allio_handle_flags
	(
		page_level_1,
		page_level_2,
	);

	using map_memory_t = map_io::map_memory_t;
	using commit_t = map_io::commit_t;
	using decommit_t = map_io::decommit_t;

	using operations = type_list_append
	<
		base_type::operations
		, map_memory_t
		, commit_t
		, decommit_t
	>;

	static vsm::result<void> map_memory(
		native_handle<map_t>& h,
		io_parameters_t<map_t, map_memory_t> const& a);

	static vsm::result<void> commit(
		native_handle<map_t> const& h,
		io_parameters_t<map_t, commit_t> const& a);

	static vsm::result<void> decommit(
		native_handle<map_t> const& h,
		io_parameters_t<map_t, decommit_t> const& a);

	static vsm::result<void> close(
		native_handle<map_t>& h,
		io_parameters_t<map_t, close_t> const& a);

	static page_level get_page_level(native_handle<map_t> const& h);

	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] void* base() const
		{
			using native_handle_type = native_handle<map_t>;
			return static_cast<Handle const&>(*this).native().native_handle_type::base;
		}

		[[nodiscard]] size_t size() const
		{
			using native_handle_type = native_handle<map_t>;
			return static_cast<Handle const&>(*this).native().native_handle_type::size;
		}

		[[nodiscard]] detail::page_level page_level() const
		{
			return get_page_level(static_cast<Handle const&>(*this).native());
		}

		[[nodiscard]] size_t page_size() const
		{
			return detail::get_page_size(page_level());
		}

		[[nodiscard]] auto commit(void* const base, size_t const size, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, commit_t>{};
			a.base = base;
			a.size = size;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<commit_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto decommit(void* const base, size_t const size, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, decommit_t>{};
			a.base = base;
			a.size = size;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<decommit_t>(static_cast<Handle const&>(*this), a);
		}
	};
};

template<>
struct native_handle<map_t> : native_handle<map_t::base_type>
{
	shared_section_handle* section;

	void* base;
	size_t size;
};

template<typename Traits>
[[nodiscard]] auto map_memory(
	size_t const size,
	auto&&... args)
{
	auto a = io_parameters_t<map_t, map_io::map_memory_t>{};
	a.options = map_options::initial_commit;
	a.size = size;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<map_t, map_io::map_memory_t>(a);
}

template<typename Traits>
[[nodiscard]] auto map_section(
	handle_for<section_t> auto const& section,
	fs_size const offset,
	size_t const size,
	auto&&... args)
{
	auto a = io_parameters_t<map_t, map_io::map_memory_t>{};
	a.options = map_options::initial_commit | map_options::backing_section;
	a.section = &section.native();
	a.section_offset = offset;
	a.size = size;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<map_t, map_io::map_memory_t>(a);
}

} // namespace allio::detail
