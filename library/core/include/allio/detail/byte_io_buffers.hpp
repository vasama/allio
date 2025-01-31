#pragma once

#include <vsm/assert.h>
#include <vsm/concepts.hpp>
#include <vsm/flags.hpp>
#include <vsm/result.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>

#include <span>

#include <cstddef>
#include <cstdint>

namespace allio::detail {

template<vsm::any_cv_of<std::byte> T>
using basic_buffer = std::span<T>;

using read_buffer = basic_buffer<std::byte>;
using write_buffer = basic_buffer<std::byte const>;


template<vsm::non_cvref T>
[[nodiscard]] read_buffer as_read_buffer(std::span<T> const span)
{
	return read_buffer(reinterpret_cast<std::byte*>(span.data()), span.size_bytes());
}

template<vsm::non_cvref T>
[[nodiscard]] read_buffer as_read_buffer(T* const data, size_t const size)
{
	return read_buffer(reinterpret_cast<std::byte*>(data), size * sizeof(T));
}

template<vsm::non_cvref T>
[[nodiscard]] write_buffer as_write_buffer(std::span<T const> const span)
{
	return write_buffer(reinterpret_cast<std::byte const*>(span.data()), span.size_bytes());
}

template<vsm::non_cvref T>
[[nodiscard]] write_buffer as_write_buffer(T const* const data, size_t const size)
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

struct new_io_buffers_view
{
	void const* buffers_data;
	size_t buffers_size;
};


template<typename T, typename U>
void _new_io_buffer_span_concept(std::span<U> const&)
	requires std::convertible_to<U*, T*>;

template<typename Span, typename Byte>
concept new_io_buffer_span_concept = requires (Span const& span)
{
	_new_io_buffer_span_concept<Byte>(span);
};

template<vsm::any_cv_of<std::byte> T>
struct get_new_io_buffer_layout_t
{
	template<std::ranges::contiguous_range Range>
		requires new_io_buffer_span_concept<std::ranges::range_value_t<Range>, T>
	[[nodiscard]] friend new_io_buffer_layout tag_invoke(
		get_new_io_buffer_layout_t,
		Range const& range)
	{
		return span_layout;
	}

	template<typename Buffer>
	[[nodiscard]] vsm_static_operator new_io_buffer_layout operator()(
		Buffer const& buffer) vsm_static_operator_const
		requires vsm::tag_invocable<get_new_io_buffer_layout_t, Buffer const&>
	{
		return vsm::tag_invoke(get_new_io_buffer_layout_t(), buffer);
	}
};

template<vsm::any_cv_of<std::byte> T>
inline constexpr get_new_io_buffer_layout_t<T> get_new_io_buffer_layout = {};

template<typename Range, typename T>
concept new_io_buffers_range_concept = requires (Range const& range)
{
	get_new_io_buffer_layout<T>(range);
};


struct new_io_buffers_base : protected new_io_buffer
{
protected:
#if 0
	static constexpr size_t layout_shift = sizeof(size_t) * CHAR_BIT - 2;

	static constexpr size_t size_data_flag =
		static_cast<size_t>(new_io_buffer_layout::size_data) << layout_shift;

	static constexpr size_t size_mask = static_cast<size_t>(-1) >> 4;
	static constexpr size_t view_flag = static_cast<size_t>(1) << (layout_shift - 1);
	static constexpr size_t size_flag = static_cast<size_t>(1) << (layout_shift - 2);
#endif

	static constexpr size_t byte_size_mask          = static_cast<size_t>(-1) >> 1;
	static constexpr size_t view_flag               = ~byte_size_mask;

	static constexpr size_t view_size_mask          = static_cast<size_t>(-1) >> 4;
	static constexpr size_t truncated_flag          = view_flag >> 1;

	static constexpr size_t layout_shift            = sizeof(size_t) * CHAR_BIT - 4;
	static constexpr size_t layout_mask             = 0b11;

	static constexpr size_t truncated_size          = view_flag | truncated_flag;

public:
	[[nodiscard]] constexpr new_io_buffer_layout get_layout() const noexcept
	{
		return m1.size & view_flag
			? static_cast<new_io_buffer_layout>(m1.size >> layout_shift & layout_mask)
			: new_io_buffer_layout::data_size;
	}

	[[nodiscard]] constexpr bool was_truncated() const noexcept
	{
		return (m1.size & truncated_size) == truncated_size;
	}

	[[nodiscard]] new_io_buffers_view get_buffers() const noexcept
	{
		if (m1.size & view_flag)
		{
			return
			{
				.buffers_data = m0.data,
				.buffers_size = m1.size & view_size_mask,
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
			return m1.size & view_size_mask;
		}
		else
		{
			return 1;
		}
	}
};

template<vsm::any_cv_of<std::byte> T>
class new_io_buffers : public new_io_buffers_base
{
public:
	constexpr new_io_buffers()
		: new_io_buffers_base{}
	{
	}

	explicit constexpr new_io_buffers(T* const data, size_t const size)
	{
		m0.data = data;
		m1.size = size > byte_size_mask ? truncated_size : size;
	}

	explicit constexpr new_io_buffers(
		new_io_buffer const* const buffers_data,
		size_t const buffers_size,
		new_io_buffer_layout const layout)
		: new_io_buffers(static_cast<void const*>(buffers_data), buffers_size, layout)
	{
	}

	template<std::ranges::contiguous_range Range>
		requires std::convertible_to<std::ranges::range_value_t<Range>*, T*>
	constexpr new_io_buffers(Range const& range)
		: new_io_buffers(std::ranges::data(range), std::ranges::size(range))
	{
	}

	template<vsm::no_cvref_of<new_io_buffers> Range>
		requires new_io_buffers_range_concept<Range, T>
	constexpr new_io_buffers(Range const& range)
		: new_io_buffers(
			static_cast<void const*>(std::ranges::data(range)),
			std::ranges::size(range),
			get_new_io_buffer_layout<T>(range))
	{
	}

	using new_io_buffers_base::get_layout;
	using new_io_buffers_base::was_truncated;
	using new_io_buffers_base::get_buffers;

private:
	explicit constexpr new_io_buffers(
		void const* const buffers_data,
		size_t const buffers_size,
		new_io_buffer_layout const layout)
	{
		size_t const size = buffers_size <= view_size_mask
			? buffers_size
			: view_size_mask | truncated_flag;

		m0.data = buffers_data;
		m1.size = size | view_flag | static_cast<size_t>(layout) << layout_shift;
	}
};

using new_read_buffers = new_io_buffers<std::byte>;
using new_write_buffers = new_io_buffers<std::byte const>;


class new_io_buffers_storage
{
	struct storage_type
	{
		size_t const size;
		new_io_buffer data[];

		explicit storage_type(size_t const size)
			: size(size)
#if vsm_compiler_msvc
			, data{}
#endif
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

	[[nodiscard]] new_io_buffers_view get_buffers_view() const
	{
		vsm_assert(m_storage != nullptr);

		return new_io_buffers_view
		{
			.buffers_data = m_storage->data,
			.buffers_size = m_storage->size,
		};
	}

	[[nodiscard]] vsm::result<new_io_buffer*> resize(size_t size) &;
};


[[nodiscard]] vsm::result<new_io_buffers_view> get_io_buffers(
	new_io_buffers_storage& storage,
	new_io_buffers_base const& buffers,
	new_io_buffer_layout required_layout);

[[nodiscard]] new_io_buffers_view get_io_buffers_unchecked(
	new_io_buffers_storage const& storage,
	new_io_buffers_base const& buffers,
	new_io_buffer_layout required_layout);

[[nodiscard]] bool io_buffers_is_empty(new_io_buffers_base buffers);

[[nodiscard]] size_t get_io_buffers_size(new_io_buffers_base buffers);

} // namespace allio::detail
