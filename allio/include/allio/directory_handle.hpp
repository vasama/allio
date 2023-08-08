#pragma once

#include <allio/async_fwd.hpp>
#include <allio/detail/api.hpp>
#include <allio/filesystem_handle.hpp>
#include <allio/multiplexer.hpp>
#include <allio/output_string_ref.hpp>

#include <vsm/box.hpp>

#include <span>

namespace allio {
namespace detail {

enum class directory_stream_native_handle : uintptr_t
{
	end                                 = 0,
	directory_end                       = static_cast<uintptr_t>(-1),
};

} // namespace detail

struct directory_entry
{
	file_kind kind;
	file_id_64 node_id;
	input_string_view name;

	vsm::result<size_t> get_name(output_string_ref output) const;

	template<typename String = std::string>
	vsm::result<String> get_name() const
	{
		String string = {};
		vsm_try_discard(get_name(string));
		return static_cast<String&&>(string);
	}
};

class directory_entry_view
{
	detail::directory_stream_native_handle m_handle;

public:
	explicit directory_entry_view(detail::directory_stream_native_handle const handle)
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
	static directory_entry get_entry(detail::directory_stream_native_handle handle);
};

class directory_stream_sentinel {};

class directory_stream_iterator
{
	detail::directory_stream_native_handle m_handle;

public:
	explicit directory_stream_iterator(detail::directory_stream_native_handle const handle)
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

	directory_stream_iterator& operator++()
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
		return m_handle == detail::directory_stream_native_handle::end;
	}

	[[nodiscard]] bool operator!=(directory_stream_sentinel) const
	{
		return m_handle != detail::directory_stream_native_handle::end;
	}

private:
	static detail::directory_stream_native_handle advance(detail::directory_stream_native_handle handle);
};

class directory_stream_view
{
	detail::directory_stream_native_handle m_handle;

public:
	directory_stream_view() = default;

	explicit directory_stream_view(detail::directory_stream_native_handle const handle)
		: m_handle(handle)
	{
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_handle != detail::directory_stream_native_handle::directory_end;
	}

	[[nodiscard]] directory_stream_iterator begin() const
	{
		vsm_assert(m_handle != detail::directory_stream_native_handle::directory_end);
		return directory_stream_iterator(m_handle);
	}

	[[nodiscard]] directory_stream_sentinel end() const
	{
		vsm_assert(m_handle != detail::directory_stream_native_handle::directory_end);
		return {};
	}
};

class directory_stream_buffer
{
public:
};


namespace io {

struct directory_read;

} // namespace io

namespace detail {

class directory_handle_base : public filesystem_handle
{
	using final_handle_type = final_handle<directory_handle_base>;

public:
	using base_type = filesystem_handle;

	using async_operations = type_list_cat<
		base_type::async_operations,
		type_list<
			io::directory_read
		>
	>;


	#define allio_directory_handle_fetch_parameters(type, data, ...) \
		type(allio::detail::directory_handle_base, read_parameters) \
		data(bool, restart, false) \

	allio_interface_parameters(allio_directory_handle_fetch_parameters);

	using get_name_parameters = basic_parameters;


	constexpr directory_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(input_path_view const path, P const& args = {})
	{
		return block_open(nullptr, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(filesystem_handle const& base, input_path_view const path, P const& args = {})
	{
		return block_open(&base, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(filesystem_handle const* const base, input_path_view const path, P const& args = {})
	{
		return block_open(base, path, args);
	}


	template<parameters<read_parameters> P = read_parameters::interface>
	vsm::result<directory_stream_view> read(std::span<std::byte> const buffer, P const& args = {})
	{
		return block_read(buffer, args);
	}

	template<parameters<directory_handle_base::read_parameters> P = read_parameters::interface>
	basic_sender<io::directory_read> read_async(std::span<std::byte> buffer, P const& args = {});


	vsm::result<file_attributes> get_attributes(directory_entry_view entry) const;

private:
	vsm::result<void> block_open(filesystem_handle const* base, input_path_view path, open_parameters const& args);

	vsm::result<directory_stream_view> block_read(std::span<std::byte> buffer, read_parameters const& args);


protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::filesystem_open> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::directory_read> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

using directory_handle = final_handle<detail::directory_handle_base>;


vsm::result<directory_handle> block_open_directory(filesystem_handle const* base, input_path_view path, directory_handle_base::open_parameters const& args);
vsm::result<directory_handle> block_open_unique_directory(filesystem_handle const& base, directory_handle_base::open_parameters const& args);
vsm::result<directory_handle> block_open_temporary_directory(directory_handle_base::open_parameters const& args);
vsm::result<directory_handle> block_open_anonymous_directory(filesystem_handle const* base, directory_handle_base::open_parameters const& args);

} // namespace detail

using detail::directory_handle;

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_directory(input_path_view const path, P const& args = {})
{
	return detail::block_open_directory(nullptr, path, args);
}

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_directory(filesystem_handle const& base, input_path_view const path, P const& args = {})
{
	return detail::block_open_directory(&base, path, args);
}

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_unique_directory(filesystem_handle const& base, P const& args = {})
{
	return detail::block_open_unique_directory(base, args);
}

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_temporary_directory(P const& args = {})
{
	return detail::block_open_temporary_directory(args);
}

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_anonymous_directory(filesystem_handle const& base, P const& args = {})
{
	return detail::block_open_anonymous_directory(base, args);
}


template<>
struct io::parameters<io::directory_read>
	: directory_handle::read_parameters
{
	using handle_type = directory_handle;
	using result_type = directory_stream_view;

	std::span<std::byte> buffer;
};

allio_detail_api extern allio_handle_implementation(directory_handle);


namespace this_process {

vsm::result<size_t> get_current_directory(output_path_ref output);

template<typename Path = path>
vsm::result<Path> get_current_directory()
{
	Path path = {};
	vsm_try_discard(get_current_directory(path));
	return static_cast<Path&&>(path);
}

vsm::result<void> set_current_directory(input_path_view path);

vsm::result<directory_handle> open_current_directory();

//TODO: open_current_directory_async(multiplexer& multiplexer);

} // namespace this_process

} // namespace allio
