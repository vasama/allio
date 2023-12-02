#pragma once

#include <allio/detail/handles/section.hpp>

namespace allio::detail {

namespace map_io {

struct map_memory_t
{
	using operation_concept = producer_t;
	struct required_params_type
	{
		void* base;
		size_t size;
		detail::protection protection;
	};
	using optional_params_type = no_parameters_t;
	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, map_memory_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, map_memory_t> const& a)
		requires requires { Object::map_memory(h, a); }
	{
		return Object::map_memory(h, a);
	}
};

struct commit_t
{
	using operation_concept = void;
	struct required_params_type
	{
		void* base;
		size_t size;
	};
	using optional_params_type = no_parameters_t;
	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, commit_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, commit_t> const& a)
		requires requires { Object::commit(h, a); }
	{
		return Object::commit(h, a);
	}
};

struct decommit_t
{
	using operation_concept = void;
	struct required_params_type
	{
		void* base;
		size_t size;
	};
	using optional_params_type = no_parameters_t;
	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, decommit_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, decommit_t> const& a)
		requires requires { Object::decommit(h, a); }
	{
		return Object::decommit(h, a);
	}
};

struct protect_t
{
	using operation_concept = void;
	struct required_params_type
	{
		void* base;
		size_t size;
		detail::protection protection;
	};
	using optional_params_type = no_parameters_t;
	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, protect_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, protect_t> const& a)
		requires requires { Object::protect(h, a); }
	{
		return Object::protect(h, a);
	}
};

} // namespace map_io

struct map_t : object_t
{
	using base_type = object_t;

	allio_handle_flags
	(
		anonymous,
	);

	struct native_type : base_type::native_type
	{
		section_t::native_type section;
		detail::page_level page_level;

		void* base;
		size_t size;
	};

	using map_memory_t = map_io::map_memory_t;
	using commit_t = map_io::commit_t;
	using decommit_t = map_io::decommit_t;
	using protect_t = map_io::protect_t;

	using operations = type_list_append
	<
		base_type::operations
		, map_memory_t
		, commit_t
		, decommit_t
		, protect_t
	>;

	static vsm::result<void> map_memory(
		native_type& h,
		io_parameters_t<map_t, map_memory_t> const& a);

	static vsm::result<void> commit(
		native_type const& h,
		io_parameters_t<map_t, commit_t> const& a);

	static vsm::result<void> decommit(
		native_type const& h,
		io_parameters_t<map_t, decommit_t> const& a);

	static vsm::result<void> protect(
		native_type const& h,
		io_parameters_t<map_t, protect_t> const& a);

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<map_t, close_t> const& a);

	template<typename Handle>
	struct abstract_interface : base_type::abstract_interface<Handle>
	{
		[[nodiscard]] void* base() const
		{
			return static_cast<Handle const&>(*this).native().native_type::base;
		}

		[[nodiscard]] size_t size() const
		{
			return static_cast<Handle const&>(*this).native().native_type::size;
		}
	
		[[nodiscard]] detail::page_level page_level() const
		{
			return static_cast<Handle const&>(*this).native().native_type::page_level;
		}
	
		[[nodiscard]] size_t page_size() const
		{
			return allio::get_page_size(page_level());
		}
	};
};
using abstract_map_handle = abstract_handle<map_t>;


namespace _map_b {

using map_handle = blocking_handle<map_t>;

vsm::result<map_handle> map_section(
	vsm::any_cvref_of<section_handle> auto&& section,
	fs_size const offset,
	size_t const size,
	auto&&... args)
{
	vsm::result<map_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<map_t, map_io::map_memory_t>(
		*r,
		make_io_args<map_t, map_io::map_memory_t>(section, offset, size)(vsm_forward(args)...)));
	return r;
}

vsm::result<map_handle> map_anonymous(size_t const size, auto&&... args)
{
	vsm::result<map_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<map_t, map_io::map_memory_t>(
		*r,
		make_io_args<map_t, map_io::map_memory_t>(nullptr, 0, size)(vsm_forward(args)...)));
	return r;
}

} // namespace _map_b

} // namespace allio::detail
