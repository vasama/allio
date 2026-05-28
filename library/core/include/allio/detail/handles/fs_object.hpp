#pragma once

#include <allio/any_byte_buffer.hpp>
#include <allio/any_path.hpp>
#include <allio/any_path_buffer.hpp>
#include <allio/detail/filesystem.hpp>
#include <allio/detail/optional_enum.hpp>
#include <allio/detail/optional_flags.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/path.hpp>
#include <allio/path_view.hpp>

namespace allio::detail {

enum class file_mode : uint8_t
{
	// Bit 0 is used by optional_flags.

	none                                = 0,

	read_data                           = 1 << 1,
	write_data                          = 1 << 2,

	read_attributes                     = 1 << 3,
	write_attributes                    = 1 << 4,

	read                                = read_data | read_attributes,
	write                               = write_data | write_attributes,
	read_write                          = read | write,

	all                                 = read_write,
};
vsm_flag_enum(file_mode);

enum class file_opening : uint8_t
{
	open_existing = 1,
	create_only,
	open_or_create,
	truncate_existing,
	replace_existing,
};

//TODO: Replace file_opening
enum class file_opening2 : uint8_t
{
	open                        = 1 << 1,
	create                      = 1 << 2,
	truncate                    = 1 << 3,
	replace                     = 1 << 4,

	open_existing               = open,
	create_only                 = create,
	open_or_create              = open | create,
	truncate_existing           = open | truncate,
	create_or_truncate          = open | truncate | create,
};

/// @brief Controls sharing of the file by other handles.
/// @note File sharing restrictions may not be available on all platforms.
///       For maximum portability, do not specify file sharing restrictions.
enum class file_sharing : uint8_t
{
	// Bit 0 is used by optional_flags.

	/// @brief Handles opened using this option prevent the file from being unlinked or opened
	///        again.
	none                                = 0,

	/// @brief Handles opened using this option do not prevent the file from being unlinked.
	unlink                              = 1 << 1,

	/// @brief Handles opened using this option do not prevent the file from being opened again for
	///        reading.
	read                                = 1 << 2,

	/// @brief Handles opened using this option do not prevent the file from being opened again for
	///        writing.
	write                               = 1 << 3,

	all = unlink | read | write,
};
vsm_flag_enum(file_sharing);

enum class file_caching : uint8_t
{
	/// @brief The OS may perform no file caching (direct I/O).
	/// @note In most scenarios this option reduces I/O performance.
	/// @note Memory and file offset alignment restrictions may apply.
	none,

	/// @brief The OS may perform general purpose caching (default).
	normal,

	/// @brief The OS may optimize for fully sequential access.
	sequential,

	/// @brief The OS may optimize for random access patterns.
	random,
};

enum class file_options : uint16_t
{
	// Bit 0 is used by optional_flags.
};
vsm_flag_enum(file_options);

enum class open_options : uint8_t
{
	temporary                           = 1 << 0,
	unique_name                         = 1 << 1,
	anonymous                           = 1 << 2,
};
vsm_flag_enum(open_options);


struct replace_existing_file_t : explicit_argument<replace_existing_file_t, bool> {};
inline constexpr explicit_parameter<replace_existing_file_t> replace_existing_file{};

namespace fs_io {

struct open_t
{
	using operation_concept = producer_t;

	struct params_type : io_flags_t
	{
		optional_flags<file_mode> mode;
		optional_flags<file_options> options;
		optional_enum<file_opening> opening;
		optional_flags<file_sharing> sharing;
		optional_enum<file_caching> caching;
		open_options special = {};
		fs_path path;

		using io_flags_t::set_argument;

		void set_argument(file_mode const value)
		{
			mode = value;
		}

		void set_argument(file_opening const value)
		{
			opening = value;
		}

		void set_argument(file_sharing const value)
		{
			sharing = value;
		}

		void set_argument(file_caching const value)
		{
			caching = value;
		}

		void set_argument(file_options const value)
		{
			options = value;
		}

		void set_argument(open_options const value)
		{
			special = value;
		}

		void set_argument(fs_path const& value)
		{
			path = value;
		}
	};

	using result_type = void;

	template<object Object>
	static vsm::result<void> blocking_io(
		native_handle<Object>& h,
		io_parameters_t<Object, open_t> const& a)
		requires requires { Object::open(h, a); }
	{
		return Object::open(h, a);
	}
};

struct query_file_info_t
{
	using operation_concept = void;

	struct params_type
	{
		optional_flags<fs_info_mask> mask;
		fs_info_buffer_ref buffer;


	};

	using result_type = fs_info_view;

	template<object Object>
	static constexpr auto& blocking_io_2 = Object::query_file_info;

	template<object Object>
	static vsm::result<fs_info_view> blocking_io(
		native_handle<Object> const& h,
		io_parameters_t<Object, query_file_info_t> const& a)
		requires requires { Object::query_file_info(h, a); }
	{
		return Object::query_file_info(h, a);
	}
};

struct get_current_path_t
{
	using operation_concept = void;

	struct params_type
	{
		any_path_buffer buffer;
		path_formats formats;

		void set_argument(path_formats const value)
		{
			formats = value;
		}

		void set_argument(path_format const format)
		{
			formats = formats | format;
		}
	};

	using result_type = size_t;

	template<object Object>
	static vsm::result<size_t> blocking_io(
		native_handle<Object> const& h,
		io_parameters_t<Object, get_current_path_t> const& a)
		requires requires { Object::get_current_path(h, a); }
	{
		return Object::get_current_path(h, a);
	}
};

struct link_at_t
{
	using operation_concept = void;

	struct params_type
	{
		fs_path path;
		bool replace_existing_file = false;

		void set_argument(explicit_parameter<replace_existing_file_t> const&)
		{
			replace_existing_file = true;
		}

		void set_argument(replace_existing_file_t const value)
		{
			replace_existing_file = value.value;
		}
	};

	using result_type = void;

	template<object Object>
	static vsm::result<void> blocking_io(
		native_handle<Object> const& h,
		io_parameters_t<Object, link_at_t> const& a)
		requires requires { Object::link_at(h, a); }
	{
		return Object::link_at(h, a);
	}
};

} // namespace fs_io

struct fs_object_t : platform_object_t
{
	using base_type = platform_object_t;

	allio_handle_flags
	(
		readable,
		writable,
	);

	using open_t = fs_io::open_t;
	using query_file_info_t = fs_io::query_file_info_t;
	using get_current_path_t = fs_io::get_current_path_t;
	using link_at_t = fs_io::link_at_t;

	using operations = type_list_append
	<
		base_type::operations
		, open_t
		, query_file_info_t
		, get_current_path_t
		, link_at_t
	>;

	static vsm::result<fs_info_view> query_file_info(
		native_handle<fs_object_t> const& h,
		io_parameters_t<fs_object_t, query_file_info_t> const& a);

	static vsm::result<size_t> get_current_path(
		native_handle<fs_object_t> const& h,
		io_parameters_t<fs_object_t, get_current_path_t> const& a);

	static vsm::result<void> link_at(
		native_handle<fs_object_t> const& h,
		io_parameters_t<fs_object_t, link_at_t> const& a);

	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		template<std::convertible_to<any_path_view> Path>
		[[nodiscard]] fs_path operator/(Path const& relative_path) const noexcept
		{
			auto const& native = static_cast<Handle const&>(*this).native();
			vsm_assert(native.flags[object_t::flags::not_null]); //PRECONDITION

			fs_path path;
			path.base = &native;
			path.path = relative_path;
			return path;
		}

		[[nodiscard]] auto query_info(any_byte_buffer_ref const buffer, auto&&... args) const
		{
			return _query_info(buffer, vsm_forward(args)...);
		}

		template<fs_info_like Information>
		[[nodiscard]] auto query_info(Information& information, auto&&... args) const
		{
			return Traits::transform(
				_query_info(fs_info_buffer_ref(information), vsm_forward(args)...),
				[](fs_info_view const info) -> void {});
		}

		[[nodiscard]] auto read_current_path(any_path_buffer const buffer, auto&&... args) const
		{
			auto r = _read_current_path(buffer, vsm_forward(args)...);

			if constexpr (Traits::has_transform_result)
			{
				return Traits::transform_result(vsm_move(r));
			}
			else
			{
				return r;
			}
		}

		template<typename Path = path>
		[[nodiscard]] auto get_current_path(auto&&... args) const
		{
			auto r = _get_current_path<Path>(vsm_forward(args)...);

			if constexpr (Traits::has_transform_result)
			{
				return Traits::transform_result(vsm_move(r));
			}
			else
			{
				return r;
			}
		}

	private:
		vsm::result<fs_info_view> _query_info(fs_info_buffer_ref const buffer, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, query_file_info_t>{};
			a.buffer = buffer;
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<query_file_info_t>(static_cast<Handle const&>(*this), a);
		}

		vsm::result<size_t> _read_current_path(any_path_buffer const buffer, auto&&... args) const
		{
			auto a = io_parameters_t<typename Handle::object_type, get_current_path_t>{};
			a.buffer = buffer;
			(set_argument(a, vsm_forward(args)), ...);
			return blocking_io<get_current_path_t>(static_cast<Handle const&>(*this), a);
		}

		template<typename Path>
		vsm::result<Path> _get_current_path(auto&&... args) const
		{
			vsm::result<Path> r(vsm::result_value);
			vsm_try(size, _read_current_path(*r, vsm_forward(args)...));
			vsm_assert(allio::get_path_string(*r).size() == size);
			return r;
		}
	};
};


template<typename Traits, detail::handle_for<fs_object_t> Handle>
[[nodiscard]] auto _link_at(Handle const& handle, fs_path const& path, auto&&... args)
{
	using operation_type = detail::fs_io::link_at_t;
	auto a = io_parameters_t<typename Handle::object_type, operation_type>{};
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template observe<operation_type>(handle, a);
}


[[nodiscard]] vsm::result<bool> _equivalent(
	native_handle<fs_object_t> const* lhs,
	native_handle<fs_object_t> const* rhs);

template<typename Traits>
[[nodiscard]] vsm::result<bool> equivalent(
	handle_for<fs_object_t> auto const& lhs,
	handle_for<fs_object_t> auto const& rhs)
{
	auto r = _equivalent(&lhs.native(), &rhs.native());

	if constexpr (Traits::has_transform_result)
	{
		return Traits::transform_result(vsm_move(r));
	}
	else
	{
		return r;
	}
}

} // namespace allio::detail
