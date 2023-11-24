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


struct directory_restart_t
{
	bool restart;
};

namespace directory_io {

struct read_t
{
	using operation_concept = void;
	struct required_params_type
	{
		read_buffer buffer;
	};
	using optional_params_type = directory_restart_t;
	using result_type = directory_stream_view;

	template<object Object>
	friend vsm::result<directory_stream_view> tag_invoke(
		blocking_io_t<Object, read_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, read_t> const& a)
		requires requires { Object::read(h, a); }
	{
		return Object::read(h, a);
	}
};

} // namespace directory_io

struct directory_t : fs_object_t
{
	using base_type = fs_object_t;

	using read_t = directory_io::read_t;

	using operations = type_list_append
	<
		base_type::operations
		, read_t
	>;

	static vsm::result<void> open(
		native_type& h,
		io_parameters_t<directory_t, open_t> const& args);

	static vsm::result<directory_stream_view> read(
		native_type const& h,
		io_parameters_t<directory_t, read_t> const& args);


	template<typename Handle, optional_multiplexer_handle_for<directory_t> MultiplexerHandle>
	struct concrete_interface : base_type::concrete_interface<Handle, MultiplexerHandle>
	{
		[[nodiscard]] auto read(read_buffer const buffer, auto&&... args) const
		{
			return generic_io<read_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, read_t>(buffer)(vsm_forward(args)...));
		}
	};
};
using abstract_directory_handle = abstract_handle<directory_t>;


namespace _directory_b {

using directory_handle = blocking_handle<directory_t>;

inline vsm::result<directory_handle> open_directory(fs_path const& path, auto&&... args)
{
	vsm::result<directory_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<directory_t, fs_io::open_t>(
		*r,
		make_io_args<directory_t, fs_io::open_t>(path)(vsm_forward(args)...)));
	return r;
}

} // namespace _directory_b

namespace _directory_a {

template<multiplexer_handle_for<directory_t> MultiplexerHandle>
using basic_directory_handle = async_handle<directory_t, MultiplexerHandle>;

ex::sender auto open_directory(fs_path const& path, auto&&... args)
{
	return io_handle_sender<directory_t, fs_io::open_t>(
		vsm_lazy(make_io_args<directory_t, fs_io::open_t>(path)(vsm_forward(args)...)));
}

} // namespace _directory_a

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/directory.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/directory.hpp>
#endif
