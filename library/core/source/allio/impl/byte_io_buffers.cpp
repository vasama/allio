#include <allio/impl/byte_io_buffers.hpp>

#include <allio/detail/byte_io_buffer_range.hpp>
#include <allio/error.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/new.hpp>

#include <bit>

using namespace allio;
using namespace allio::detail;

namespace {

template<io_buffer_layout Layout>
using layout_constant = std::integral_constant<io_buffer_layout, Layout>;

template<io_buffer_layout... Layouts>
static auto with_constant_layouts(auto const& lambda)
{
	return lambda(layout_constant<Layouts>()...);
}

template<io_buffer_layout... Layouts, std::same_as<io_buffer_layout>... Rest>
static auto with_constant_layouts(
	auto const& lambda,
	io_buffer_layout const layout,
	Rest const... rest)
{
	//TODO: Eliminate extra branches on platforms where size_data and size_le32 are not relevant.

	using enum io_buffer_layout;

	switch (std::to_underlying(layout))
	{
	case std::to_underlying(data_size):
		return with_constant_layouts<Layouts..., data_size>(lambda, rest...);

	case std::to_underlying(size_data):
		return with_constant_layouts<Layouts..., size_data>(lambda, rest...);

	case std::to_underlying(data_size | size_le32):
		return with_constant_layouts<Layouts..., data_size | size_le32>(lambda, rest...);

	case std::to_underlying(size_data | size_le32):
		return with_constant_layouts<Layouts..., size_data | size_le32>(lambda, rest...);
	}

	vsm_unreachable();
}


static constexpr bool is_size_data(io_buffer_layout const layout)
{
	return vsm::any_flags(layout, io_buffer_layout::size_data);
}

static constexpr bool is_size_le32(io_buffer_layout const layout)
{
	return vsm::any_flags(layout, io_buffer_layout::size_le32);
}

template<io_buffer_layout Layout, vsm::any_cv_of<io_buffer> Buffer>
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

static io_buffer read_io_buffer(void const* const src_buffer)
{
	io_buffer dst_buffer;
	std::memcpy(&dst_buffer, src_buffer, sizeof(io_buffer));
	return dst_buffer;
}

template<io_buffer_layout SrcLayout, io_buffer_layout DstLayout>
	requires (SrcLayout == DstLayout)
static vsm::result<void const*> swizzle_buffers_1(
	io_buffers_view const src_buffers,
	storage_provider_ref const storage_provider)
{
	vsm_unreachable();
}

template<io_buffer_layout SrcLayout, io_buffer_layout DstLayout>
static vsm::result<void const*> swizzle_buffers_1(
	io_buffers_view const src_buffers,
	storage_provider_ref const storage_provider)
{
	static constexpr bool swizzle = is_size_data(SrcLayout) != is_size_data(DstLayout);
	static constexpr bool truncate = is_size_le32(DstLayout) && !is_size_le32(SrcLayout);

	auto src_buffers_data = static_cast<unsigned char const*>(src_buffers.buffers_data);
	auto dst_buffers_data = src_buffers.buffers_data;

	io_buffer* out_buffers_data;
	if constexpr (swizzle)
	{
		vsm_try(storage, storage_provider.get_storage(
			src_buffers.buffers_size * sizeof(io_buffer),
			std::align_val_t(alignof(io_buffer))));

		out_buffers_data = static_cast<io_buffer*>(storage);
		dst_buffers_data = out_buffers_data;
	}

	for (size_t i = 0; i < src_buffers.buffers_size; ++i)
	{
		auto const src_buffer = read_io_buffer(src_buffers_data);
		src_buffers_data += sizeof(io_buffer);

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
			auto const dst_buffer = new (out_buffers_data++) io_buffer;

			dst_buffer->m0 = src_buffer.m1;
			dst_buffer->m1 = src_buffer.m0;

			//TODO: Have another look at io buffer swizzling on big-endian platforms.

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
	io_buffers_view const src_buffers,
	io_buffer_layout const src_layout,
	io_buffer_layout const dst_layout,
	storage_provider_ref const storage_provider)
{
	auto const lambda = [&]<io_buffer_layout... Layouts>(
		layout_constant<Layouts>...) -> vsm::result<void const*>
	{
		if constexpr (vsm::all_same_v<layout_constant<Layouts>...>)
		{
			vsm_unreachable();
		}
		else
		{
			return swizzle_buffers_1<Layouts...>(src_buffers, storage_provider);
		}
	};

	return with_constant_layouts(lambda, src_layout, dst_layout);
}

} // namespace

vsm::result<io_buffers_view> allio::get_io_buffers(
	io_buffers_base const& buffers,
	io_buffer_layout const required_layout,
	storage_provider_ref const storage_provider)
{
	auto const buffers_layout = buffers.get_layout();
	auto const buffers_view = buffers.get_buffers();

	if (buffers_layout == required_layout)
	{
		return buffers_view;
	}

	vsm_try(swizzled_buffers, swizzle_buffers(
		buffers_view,
		buffers_layout,
		required_layout,
		storage_provider));

	return io_buffers_view
	{
		.buffers_data = swizzled_buffers,
		.buffers_size = buffers_view.buffers_size,
	};
}

bool allio::io_buffers_is_empty(io_buffers_base const buffers)
{
	auto const lambda = [&]<io_buffer_layout Layout>(layout_constant<Layout>)
	{
		for (io_buffer const buffer : read_io_buffers(buffers.get_buffers()))
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

size_t allio::get_io_buffers_size(io_buffers_base const buffers)
{
	auto const lambda = [&]<io_buffer_layout Layout>(layout_constant<Layout>)
	{
		size_t size = 0;
		for (io_buffer const buffer : read_io_buffers(buffers.get_buffers()))
		{
			size += get_size<Layout>(buffer);
		}
		return size;
	};

	return with_constant_layouts(lambda, buffers.get_layout());
}
