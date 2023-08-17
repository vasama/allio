#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/filesystem_handle.hpp>

#include <vsm/box.hpp>

namespace allio {
namespace detail {

enum class directory_stream_native_handle : uintptr_t
{
	end                                 = 0,
	directory_end                       = static_cast<uintptr_t>(-1),
};

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
};

class _directory_handle : public filesystem_handle
{
protected:
	using base_type = filesystem_handle;

public:
	struct read_t;

	#define allio_directory_handle_fetch_parameters(type, data, ...) \
		type(allio::detail::directory_handle_base, read_parameters) \
		data(bool,                  restart,        false) \
		data(::allio::deadline,     deadline)

	allio_interface_parameters(allio_directory_handle_fetch_parameters);


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			read_t
		>
	>;

protected:
	allio_detail_default_lifetime(_directory_handle);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<read_parameters> P = read_parameters::interface>
		vsm::result<directory_stream_view> read(read_buffer const buffer, P const& args = {}) const;
	};

	template<typename M, typename H>
	struct async_interface : base_type::async_interface<M, H>
	{
		template<parameters<read_parameters> P = read_parameters::interface>
		basic_sender<M, H, read_t> read_async(read_buffer const buffer, P const& args = {}) const;
	};
};

} // namespace detail

template<>
struct io_operation_traits<directory_handle::read_t>
{
	using handle_type = directory_handle const;
	using result_type = directory_stream_view;
	using params_type = directory_handle::read_parameters;

	read_buffer buffer;
};

} // namespace allio
