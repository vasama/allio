#include <allio/byte_io_buffers.hpp>

#include <allio/detail/byte_io_buffer_range.hpp>
#include <allio/error.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/new.hpp>

#include <bit>

using namespace allio;
using namespace allio::detail;

namespace {

template<new_io_buffer_layout Layout>
using layout_constant = std::integral_constant<new_io_buffer_layout, Layout>;

template<new_io_buffer_layout... Layouts>
static auto with_constant_layouts(auto const& lambda)
{
	return lambda(layout_constant<Layouts>()...);
}

template<new_io_buffer_layout... Layouts, std::same_as<new_io_buffer_layout>... Rest>
static auto with_constant_layouts(
	auto const& lambda,
	new_io_buffer_layout const layout,
	Rest const... rest)
{
	//TODO: Eliminate extra branches on platforms where size_data and size_le32 are not relevant.

	using enum new_io_buffer_layout;

	switch (layout)
	{
		vsm_msvc_warning(push)
		vsm_msvc_warning(disable: 4063) // C4063: case x is not a valid value for switch of enum

		vsm_gnu_diagnostic(push)
		vsm_gnu_diagnostic(ignored "-Wswitch")

	case data_size:
		return with_constant_layouts<Layouts..., data_size>(lambda, rest...);

	case size_data:
		return with_constant_layouts<Layouts..., size_data>(lambda, rest...);

	case data_size | size_le32:
		return with_constant_layouts<Layouts..., data_size | size_le32>(lambda, rest...);

	case size_data | size_le32:
		return with_constant_layouts<Layouts..., size_data | size_le32>(lambda, rest...);

		vsm_msvc_warning(pop)
		vsm_gnu_diagnostic(pop)
	}

	vsm_unreachable();
}


static constexpr bool is_size_data(new_io_buffer_layout const layout)
{
	return vsm::any_flags(layout, new_io_buffer_layout::size_data);
}

static constexpr bool is_size_le32(new_io_buffer_layout const layout)
{
	return vsm::any_flags(layout, new_io_buffer_layout::size_le32);
}

template<new_io_buffer_layout Layout, vsm::any_cv_of<new_io_buffer> Buffer>
static vsm::copy_cv_t<Buffer, size_t>& get_size(Buffer& buffer)
{
	if constexpr (is_size_data(Layout))
	{
		return buffer.m0.size;
	}
	else
	{
		return buffer.m1.size;
	}
}

static new_io_buffer read_io_buffer(void const* const src_buffer)
{
	new_io_buffer dst_buffer;
	std::memcpy(&dst_buffer, src_buffer, sizeof(new_io_buffer));
	return dst_buffer;
}

template<new_io_buffer_layout SrcLayout, new_io_buffer_layout DstLayout>
	requires (SrcLayout == DstLayout)
static vsm::result<void const*> swizzle_buffers_1(
	new_io_buffers_storage& storage,
	new_io_buffers_view const src_buffers)
{
	vsm_unreachable();
}

template<new_io_buffer_layout SrcLayout, new_io_buffer_layout DstLayout>
static vsm::result<void const*> swizzle_buffers_1(
	new_io_buffers_storage& storage,
	new_io_buffers_view const src_buffers)
{
	static constexpr bool swizzle = is_size_data(SrcLayout) != is_size_data(DstLayout);
	static constexpr bool truncate = is_size_le32(DstLayout) && !is_size_le32(SrcLayout);

	auto src_buffers_data = static_cast<unsigned char const*>(src_buffers.buffers_data);
	auto dst_buffers_data = src_buffers.buffers_data;

	new_io_buffer* out_buffers_data;
	if constexpr (swizzle)
	{
		vsm_try_assign(out_buffers_data, storage.resize(src_buffers.buffers_size));
		dst_buffers_data = out_buffers_data;
	}

	for (size_t i = 0; i < src_buffers.buffers_size; ++i)
	{
		auto const src_buffer = read_io_buffer(src_buffers_data);
		src_buffers_data += sizeof(new_io_buffer);

		if constexpr (truncate)
		{
			if (get_size<SrcLayout>(src_buffer) > std::numeric_limits<uint32_t>::max())
			{
				//TODO: Return a more specific error code.
				return vsm::unexpected(allio_error(error::invalid_argument));
			}
		}

		if constexpr (swizzle)
		{
			auto const dst_buffer = new (out_buffers_data++) new_io_buffer;

			dst_buffer->m0 = src_buffer.m1;
			dst_buffer->m1 = src_buffer.m0;

#if 0
			// Non-little-endian architectures require a transforming the bytes of the size member
			// when truncating. On little-endian architectures, the low-address bytes contain the
			// least significant bits of the integer, and so no transformation is necessary.
			if constexpr (truncate && std::endian::native != std::endian::little)
			{
				auto& u = get_size<DstLayout>(*dst_buffer);
				u.size_le32 = static_cast<uint32_t>(u.size);
			}
#endif
		}
	}

	return dst_buffers_data;
}

static vsm::result<void const*> swizzle_buffers(
	new_io_buffers_storage& storage,
	new_io_buffers_view const src_buffers,
	new_io_buffer_layout const src_layout,
	new_io_buffer_layout const dst_layout)
{
	auto const lambda = [&]<new_io_buffer_layout... Layouts>(
		layout_constant<Layouts>...) -> vsm::result<void const*>
	{
		if constexpr (vsm::all_same_v<layout_constant<Layouts>...>)
		{
			vsm_unreachable();
		}
		else
		{
			return swizzle_buffers_1<Layouts...>(storage, src_buffers);
		}
	};

	return with_constant_layouts(lambda, src_layout, dst_layout);
}

} // namespace

detail::new_io_buffers_storage::~new_io_buffers_storage()
{
	static constexpr size_t data_offset = offsetof(storage_type, data);

	if (m_storage != nullptr)
	{
		detail::release_storage(
			m_storage,
			data_offset + m_storage->size * sizeof(new_io_buffer),
			alignof(storage_type),
			allio_allocation_strategy_generic);
	}
}

vsm::result<new_io_buffer*> detail::new_io_buffers_storage::resize(size_t const size) &
{
	static constexpr size_t data_offset = offsetof(storage_type, data);

	size_t const allocation_size = data_offset + size * sizeof(new_io_buffer);

	auto const allocation = detail::acquire_storage(
		/* min_size: */ allocation_size,
		/* max_size: */ allocation_size,
		alignof(new_io_buffer),
		allio_allocation_strategy_generic);

	if (allocation.storage == nullptr)
	{
		return vsm::unexpected(allio_error(error::not_enough_memory));
	}

	m_storage = new (allocation.storage) storage_type(size);

	return m_storage->data;
}

vsm::result<new_io_buffers_view> detail::get_io_buffers(
	new_io_buffers_storage& storage,
	new_io_buffers_base const& buffers,
	new_io_buffer_layout const required_layout)
{
	auto const buffers_layout = buffers.get_layout();
	auto const buffers_view = buffers.get_buffers();

	if (buffers_layout == required_layout)
	{
		return buffers_view;
	}

	vsm_try(swizzled_buffers, swizzle_buffers(
		storage,
		buffers_view,
		buffers_layout,
		required_layout));

	return new_io_buffers_view
	{
		.buffers_data = swizzled_buffers,
		.buffers_size = buffers_view.buffers_size,
	};
}

new_io_buffers_view detail::get_io_buffers_unchecked(
	new_io_buffers_storage const& storage,
	new_io_buffers_base const& buffers,
	new_io_buffer_layout const required_layout)
{
	auto const buffers_layout = buffers.get_layout();

	if (buffers_layout == required_layout)
	{
		return buffers.get_buffers();
	}

	return storage.get_buffers_view();
}

bool detail::io_buffers_is_empty(new_io_buffers_base const buffers)
{
	auto const lambda = [&]<new_io_buffer_layout Layout>(layout_constant<Layout>)
	{
		for (new_io_buffer const buffer : read_io_buffers(buffers.get_buffers()))
		{
			if (get_size<Layout>(buffer) != 0)
			{
				return false;
			}
		}
		return true;
	};

	return with_constant_layouts(lambda, buffers.get_layout());
}

size_t detail::get_io_buffers_size(new_io_buffers_base const buffers)
{
	auto const lambda = [&]<new_io_buffer_layout Layout>(layout_constant<Layout>)
	{
		size_t size = 0;
		for (new_io_buffer const buffer : read_io_buffers(buffers.get_buffers()))
		{
			size += get_size<Layout>(buffer);
		}
		return size;
	};

	return with_constant_layouts(lambda, buffers.get_layout());
}
