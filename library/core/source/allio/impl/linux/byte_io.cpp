#include <allio/impl/linux/byte_io.hpp>

#include <allio/impl/byte_io_buffers.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/poll.hpp>
#include <allio/impl/linux/signal.hpp>
#include <allio/linux/handles/platform_object.hpp>
#include <allio/linux/timespec.hpp>
#include <allio/step_deadline.hpp>

#include <vsm/numeric.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

namespace {

static constexpr fs_size max_file_extent = std::numeric_limits<off_t>::max();

template<auto Syscall, short Event, bool HandleSignal, typename Arguments>
static vsm::result<void> do_byte_io_2(
	native_handle<platform_object_t> const& h,
	Arguments const& a,
	size_t& transferred,
	std::same_as<fs_size> auto const... offset)
{
	static constexpr bool is_random_access = sizeof...(offset) != 0;

	if (io_buffers_is_empty(a.buffers))
	{
		return {};
	}

	vsm_try_void(check_io_vectors_size(a.buffers));

	dynamic_io_vector_storage storage;
	vsm_try(io_vectors, get_io_vectors(a.buffers, storage));

	int const fd = unwrap_handle(h.platform_handle);

	bool const do_poll =
		a.deadline != deadline::never() ||
		h.flags[platform_object_t::impl_type::flags::non_blocking];

	step_deadline deadline = a.deadline;

	size_t io_vector_offset = 0;
	iovec local_io_vector_storage[1];

	auto const get_local_io_vectors = [&]() -> std::span<iovec const>
	{
		vsm_assert(!io_vectors.empty());

		if (io_vector_offset == 0)
		{
			return io_vectors;
		}

		iovec& v = local_io_vector_storage[0] = io_vectors.front();
		v.iov_base = static_cast<unsigned char*>(v.iov_base) + io_vector_offset;
		v.iov_len += io_vector_offset;

		return local_io_vector_storage;
	};

	vsm::select_t<HandleSignal, sigpipe_handler, char> signal_handler;

	while (true)
	{
		if (do_poll)
		{
			vsm_try(local_deadline, deadline.step());

			vsm_try_discard(poll(
				fd,
				Event,
				local_deadline));
		}

		if constexpr (HandleSignal)
		{
			vsm_try_void(signal_handler.activate());
		}

		auto const local_io_vectors = get_local_io_vectors();
		ssize_t const r = Syscall(
			fd,
			local_io_vectors.data(),
			vsm::truncating(local_io_vectors.size()),
			vsm::truncating(offset + transferred)...);

		if (r == -1)
		{
			int const e = errno;

			if constexpr (HandleSignal)
			{
				if (e == EPIPE)
				{
					signal_handler.expect_signal();
				}
			}

			return vsm::unexpected(allio_error(static_cast<system_error>(e)));
		}

		if (r == 0)
		{
			return vsm::unexpected(allio_error(error::end_of_stream));
		}

		size_t local_transferred = static_cast<size_t>(r);
		transferred += local_transferred;

		if (vsm::no_flags(a.flags, io_flags::greedy_byte_io))
		{
			return {};
		}

		if constexpr (is_random_access)
		{
			//TODO: Figure this one out...
			if (a.offset >= max_file_extent - transferred)
			{
				return vsm::unexpected(allio_error(error::invariant_violation));
			}
		}

		while (true)
		{
			if (io_vectors.empty())
			{
				return {};
			}

			size_t io_vector_size = io_vectors.front().iov_len;

			if (io_vector_offset != 0)
			{
				vsm_assert(io_vector_offset < io_vector_size);
				io_vector_size -= io_vector_offset;
				io_vector_offset = 0;
			}

			if (io_vector_size > local_transferred)
			{
				io_vector_offset = local_transferred;
				break;
			}

			local_transferred -= io_vector_size;
			io_vectors = io_vectors.subspan(1);
		}
	}

	return {};
}

template<auto Syscall, short Event, bool HandleSignal, typename Arguments>
static vsm::result<void> do_byte_io_1(
	native_handle<platform_object_t> const& h,
	Arguments const& a,
	size_t& transferred)
{
	if constexpr (requires { a.offset; })
	{
		if (vsm::loses_precision<off_t>(a.offset))
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		return do_byte_io_2<Syscall, Event, HandleSignal>(h, a, transferred, a.offset);
	}
	else
	{
		return do_byte_io_2<Syscall, Event, HandleSignal>(h, a, transferred);
	}
}

template<auto Syscall, short Event, bool HandleSignal = false, typename Arguments>
static vsm::result<size_t> do_byte_io(native_handle<platform_object_t> const& h, Arguments const& a)
{
	size_t transferred = 0;

	if (auto const r = do_byte_io_1<Syscall, Event, HandleSignal>(h, a, transferred); !r)
	{
		// The error is ignored if some data was transferred and greedy I/O was not requested.
		if (transferred == 0 || vsm::any_flags(a.flags, io_flags::greedy_byte_io))
		{
			return vsm::unexpected(r.error());
		}
	}

	return transferred;
}

} // namespace

vsm::result<size_t> linux::random_read(
	detail::native_handle<platform_object_t> const& h,
	detail::byte_io::random_parameters_t<std::byte> const& a)
{
	return do_byte_io<preadv, POLLIN>(h, a);
}

vsm::result<size_t> linux::random_write(
	detail::native_handle<platform_object_t> const& h,
	detail::byte_io::random_parameters_t<std::byte const> const& a)
{
	return do_byte_io<pwritev, POLLOUT>(h, a);
}

vsm::result<size_t> linux::stream_read(
	detail::native_handle<platform_object_t> const& h,
	detail::byte_io::stream_parameters_t<std::byte> const& a)
{
	return do_byte_io<readv, POLLIN>(h, a);
}

vsm::result<size_t> linux::stream_write(
	detail::native_handle<platform_object_t> const& h,
	detail::byte_io::stream_parameters_t<std::byte const> const& a)
{
	return do_byte_io<writev, POLLOUT>(h, a);
}

vsm::result<size_t> linux::stream_write_no_signal(
	detail::native_handle<platform_object_t> const& h,
	detail::byte_io::stream_parameters_t<std::byte const> const& a)
{
	return do_byte_io<writev, POLLOUT, /* HandleSignal: */ true>(h, a);
}
