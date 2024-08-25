#pragma once

#include <vsm/assert.h>
#include <vsm/concepts.hpp>
#include <vsm/flags.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>

#include <span>

#include <cstddef>
#include <cstdint>

namespace allio::detail {

template<vsm::any_cv_of<std::byte> T>
using basic_buffer = std::span<T>;

using read_buffer = basic_buffer<std::byte>;
using write_buffer = basic_buffer<std::byte const>;


template<vsm::non_cvref T>
read_buffer as_read_buffer(std::span<T> const span)
{
	return read_buffer(reinterpret_cast<std::byte*>(span.data()), span.size_bytes());
}

template<vsm::non_cvref T>
read_buffer as_read_buffer(T* const data, size_t const size)
{
	return read_buffer(reinterpret_cast<std::byte*>(data), size * sizeof(T));
}

template<vsm::non_cvref T>
write_buffer as_write_buffer(std::span<T const> const span)
{
	return write_buffer(reinterpret_cast<std::byte const*>(span.data()), span.size_bytes());
}

template<vsm::non_cvref T>
write_buffer as_write_buffer(T const* const data, size_t const size)
{
	return write_buffer(reinterpret_cast<std::byte const*>(data), size * sizeof(T));
}


enum class new_io_buffer_layout : uint8_t
{
	data_size                   = 0,
	size_data                   = 1 << 0,
	size_le32                   = 1 << 1,
};
vsm_flag_enum(new_io_buffer_layout);

inline constexpr new_io_buffer_layout span_layout = new_io_buffer_layout::data_size;


struct new_io_buffer
{
	union union_type
	{
		void const* data;
		size_t size;
	};

	union_type m0;
	union_type m1;
};

struct new_io_buffers
{
	void const* buffers_data;
	size_t buffers_size;
};


template<typename Range>
concept _new_io_buffer_range =
	std::ranges::contiguous_range<Range> &&
	vsm::any_cv_of<std::ranges::range_value_t<Range>, new_io_buffer> &&
	requires (Range const& range)
	{
		{ range.get_layout() } -> std::same_as<new_io_buffer_layout>;
	};


#if 0
struct get_new_io_buffer_layout_t
{
	template<typename Buffer>
	[[nodiscard]] vsm_static_operator new_io_buffer_layout operator()(
		Buffer const& buffer) vsm_static_operator_const
	{
	}
};
inline constexpr get_new_io_buffer_layout_t get_new_io_buffer_layout = {};
#endif


struct new_io_buffers_view_base : protected new_io_buffer
{
protected:
	static constexpr size_t layout_shift = sizeof(size_t) * CHAR_BIT - 2;

	static constexpr size_t size_data_flag =
		static_cast<size_t>(new_io_buffer_layout::size_data) << layout_shift;

	static constexpr size_t size_mask = static_cast<size_t>(-1) >> 4;
	static constexpr size_t view_flag = static_cast<size_t>(1) << (layout_shift - 1);
	static constexpr size_t size_flag = static_cast<size_t>(1) << (layout_shift - 2);

	constexpr new_io_buffers_view_base()
		: new_io_buffer{}
	{
	}

	explicit constexpr new_io_buffers_view_base(
		new_io_buffer::union_type const m0,
		new_io_buffer::union_type const m1)
		: new_io_buffer{ m0, m1 }
	{
	}

public:
	[[nodiscard]] constexpr new_io_buffer_layout get_layout() const noexcept
	{
		return static_cast<new_io_buffer_layout>(m1.size >> layout_shift);
	}

	[[nodiscard]] constexpr bool was_truncated() const noexcept
	{
		return m1.size & size_flag;
	}

	[[nodiscard]] new_io_buffers get_buffers() const noexcept
	{
		if (m1.size & view_flag)
		{
			return
			{
				.buffers_data = m0.data,
				.buffers_size = m1.size & size_mask,
			};
		}
		else
		{
			return
			{
				.buffers_data = static_cast<new_io_buffer const*>(this),
				.buffers_size = 1,
			};
		}
	}

	[[nodiscard]] size_t get_buffers_size() const
	{
		if (m1.size & view_flag)
		{
			return m1.size & size_mask;
		}
		else
		{
			return 1;
		}
	}
};

template<vsm::any_cv_of<std::byte> T>
class new_io_buffers_view : public new_io_buffers_view_base
{
public:
	new_io_buffers_view() = default;

	explicit constexpr new_io_buffers_view(
		T* const data,
		size_t const size)
		: new_io_buffers_view_base(
			new_io_buffer::union_type{ .data = data },
			new_io_buffer::union_type{ .size = make_size(size, new_io_buffer_layout::data_size) })
	{
	}

	explicit constexpr new_io_buffers_view(
		new_io_buffer const* const buffers_data,
		size_t const buffers_size,
		new_io_buffer_layout const layout)
		: new_io_buffers_view(static_cast<void const*>(buffers_data), buffers_size, layout)
	{
	}

	template<std::ranges::contiguous_range Range>
		requires std::convertible_to<std::ranges::range_value_t<Range>*, T*>
	constexpr new_io_buffers_view(Range const& range)
		: new_io_buffers_view(std::ranges::data(range), std::ranges::size(range))
	{
	}

	template<vsm::no_cvref_of<new_io_buffers_view> Range>
		requires _new_io_buffer_range<Range>
	constexpr new_io_buffers_view(Range const& range)
		: new_io_buffers_view(
			static_cast<void const*>(std::ranges::data(range)),
			std::ranges::size(range),
			range.get_layout())
	{
	}

	template<std::ranges::contiguous_range Range>
		requires std::same_as<std::ranges::range_value_t<Range>, std::span<T>>
	constexpr new_io_buffers_view(Range const& range)
		: new_io_buffers_view(
			static_cast<void const*>(std::ranges::data(range)),
			std::ranges::size(range),
			span_layout)
	{
	}

	using new_io_buffers_view_base::get_layout;
	using new_io_buffers_view_base::was_truncated;
	using new_io_buffers_view_base::get_buffers;

private:
	explicit constexpr new_io_buffers_view(
		void const* const buffers_data,
		size_t const buffers_size,
		new_io_buffer_layout const layout)
		: new_io_buffers_view_base(
			new_io_buffer::union_type{ .data = buffers_data },
			new_io_buffer::union_type{ .size = make_size(buffers_size, layout) | view_flag })
	{
	}

	[[nodiscard]] static constexpr size_t make_size(
		size_t const size,
		new_io_buffer_layout const layout) noexcept
	{
		return size > size_mask ? size_flag : size | static_cast<size_t>(layout) << layout_shift;
	}
};

using new_read_buffers = new_io_buffers_view<std::byte>;
using new_write_buffers = new_io_buffers_view<std::byte const>;


class new_io_buffers_storage
{
	struct storage_type
	{
		size_t const size;
		new_io_buffer data[];

		explicit storage_type(size_t const size)
			: size(size)
			, data{}
		{
		}
	};

	storage_type* m_storage;

public:
	new_io_buffers_storage()
		: m_storage(nullptr)
	{
	}

	new_io_buffers_storage(new_io_buffers_storage const&) = delete;
	new_io_buffers_storage& operator=(new_io_buffers_storage const&) = delete;

	~new_io_buffers_storage();

	vsm::result<new_io_buffer*> resize(size_t size) &;
};


//TODO: Move these to a separate implementation header:

[[nodiscard]] vsm::result<new_io_buffers> get_io_buffers(
	new_io_buffers_storage& storage,
	new_io_buffers_view_base const& view,
	new_io_buffer_layout required_layout);

[[nodiscard]] size_t get_io_buffers_size(new_io_buffers_view_base buffers);

} // namespace allio::detail
