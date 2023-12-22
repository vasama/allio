#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/fs_object.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/box.hpp>

namespace allio::detail {

enum class directory_stream_native_handle : uintptr_t
{
	end                                 = 0,
	directory_end                       = static_cast<uintptr_t>(-1),
};

//TODO: Template on error traits
struct directory_entry
{
	fs_entry_type type;
	fs_node_id node_id;
	any_string_view name;

	vsm::result<size_t> get_name(any_string_buffer buffer) const;

	template<typename String = std::string>
	vsm::result<String> get_name() const
	{
		vsm::result<String> r(vsm::result_value);
		vsm_try_discard(get_name(*r));
		return r;
	}
};

class directory_entry_view
{
	directory_stream_native_handle m_handle;

public:
	explicit directory_entry_view(directory_stream_native_handle const handle)
		: m_handle(handle)
	{
	}

	[[nodiscard]] directory_entry get_entry() const
	{
		return get_entry(m_handle);
	}

	[[nodiscard]] operator directory_entry() const
	{
		return get_entry(m_handle);
	}

private:
	static directory_entry get_entry(directory_stream_native_handle handle);
};

class directory_stream_sentinel {};

class directory_stream_iterator
{
	directory_stream_native_handle m_handle;

public:
	explicit directory_stream_iterator(directory_stream_native_handle const handle)
		: m_handle(handle)
	{
	}

	[[nodiscard]] directory_entry_view operator*() const
	{
		return directory_entry_view(m_handle);
	}

	[[nodiscard]] vsm::box<directory_entry_view> operator->() const
	{
		return directory_entry_view(m_handle);
	}

	directory_stream_iterator& operator++() &
	{
		m_handle = advance(m_handle);
		return *this;
	}

	[[nodiscard]] directory_stream_iterator operator++(int) &
	{
		auto it = *this;
		m_handle = advance(m_handle);
		return *this;
	}

	[[nodiscard]] bool operator==(directory_stream_sentinel) const
	{
		return m_handle == directory_stream_native_handle::end;
	}

	[[nodiscard]] bool operator!=(directory_stream_sentinel) const
	{
		return m_handle != directory_stream_native_handle::end;
	}

private:
	static directory_stream_native_handle advance(directory_stream_native_handle handle);
};

class directory_stream_view
{
	directory_stream_native_handle m_handle;

public:
	directory_stream_view() = default;

	explicit directory_stream_view(directory_stream_native_handle const handle)
		: m_handle(handle)
	{
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_handle != directory_stream_native_handle::directory_end;
	}

	[[nodiscard]] directory_stream_iterator begin() const
	{
		vsm_assert(m_handle != directory_stream_native_handle::directory_end);
		return directory_stream_iterator(m_handle);
	}

	[[nodiscard]] directory_stream_sentinel end() const
	{
		vsm_assert(m_handle != directory_stream_native_handle::directory_end);
		return {};
	}
};


namespace directory_io {

struct read_t
{
	using operation_concept = void;

	struct params_type
	{
		read_buffer buffer;
	};

	using result_type = directory_stream_view;

	template<object Object>
	friend vsm::result<directory_stream_view> tag_invoke(
		blocking_io_t<read_t>,
		native_handle<Object> const& h,
		io_parameters_t<Object, read_t> const& a)
		requires requires { Object::read(h, a); }
	{
		return Object::read(h, a);
	}
};

struct restart_t
{
	using operation_concept = void;
	using params_type = no_parameters_t;
	using result_type = void;

	template<object Object>
	friend vsm::result<directory_stream_view> tag_invoke(
		blocking_io_t<restart_t>,
		native_handle<Object> const& h,
		io_parameters_t<Object, restart_t> const& a)
		requires requires { Object::restart(h, a); }
	{
		return Object::restart(h, a);
	}
};

} // namespace directory_io

struct directory_t : fs_object_t
{
	using base_type = fs_object_t;

	using read_t = directory_io::read_t;
	using restart_t = directory_io::restart_t;

	using operations = type_list_append
	<
		base_type::operations
		, read_t
		, restart_t
	>;

	static vsm::result<void> open(
		native_handle<directory_t>& h,
		io_parameters_t<directory_t, open_t> const& a);

	static vsm::result<directory_stream_view> read(
		native_handle<directory_t> const& h,
		io_parameters_t<directory_t, read_t> const& a);

	static vsm::result<void> restart(
		native_handle<directory_t>& h,
		io_parameters_t<directory_t, restart_t> const& a);


	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] auto read(read_buffer const buffer) const
		{
			auto a = io_parameters_t<typename Handle::object_type, read_t>{};
			a.buffer = buffer;
			return Traits::template observe<read_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto restart() const
		{
			return Traits::template observe<restart_t>(
				static_cast<Handle const&>(*this),
				no_parameters_t());
		}
	};
};

template<typename Traits>
[[nodiscard]] auto open_directory(fs_path const& path, auto&&... args)
{
	auto a = io_parameters_t<directory_t, fs_io::open_t>{};
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<directory_t, fs_io::open_t>(a);
}

template<typename Traits>
[[nodiscard]] auto open_temp_directory(auto&&... args)
{
	auto a = io_parameters_t<directory_t, fs_io::open_t>{};
	a.special = open_options::temporary;
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<directory_t, fs_io::open_t>(a);
}

template<typename Traits>
[[nodiscard]] auto open_unique_directory(auto&&... args)
{
	auto a = io_parameters_t<directory_t, fs_io::open_t>{};
	a.special = open_options::unique_name;
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<directory_t, fs_io::open_t>(a);
}

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/directory.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/directory.hpp>
#endif
