#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/filesystem_handle.hpp>

#include <vsm/box.hpp>

namespace allio::detail {

enum class directory_stream_native_handle : uintptr_t
{
	end                                 = 0,
	directory_end                       = static_cast<uintptr_t>(-1),
};

struct directory_entry
{
	file_kind kind;
	file_id_64 node_id;
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


struct directory_restart_t
{
	bool restart;

	struct tag_t
	{
		struct argument_t
		{
			bool value;

			friend void tag_invoke(set_argument_t, directory_restart_t& args, argument_t const& value)
			{
				args.restart = value.value;
			}
		};

		argument_t vsm_static_operator_invoke(bool const restart)
		{
			return { restart };
		}

		friend void tag_invoke(set_argument_t, directory_restart_t& args, tag_t)
		{
			args.restart = true;
		}
	};
};
inline constexpr directory_restart_t::tag_t directory_restart = {};

struct directory_t : fs_object_t
{
	using base_type = fs_object_t;

	struct read_t
	{
		using result_type = directory_stream_view;

		struct required_params_type
		{
			read_buffer buffer;
		};

		using optional_params_type = directory_restart_t;
	};

	using operations = type_list_cat
	<
		base_type::operations,
		type_list<read_t>
	>;
};

} // namespace allio::detail
