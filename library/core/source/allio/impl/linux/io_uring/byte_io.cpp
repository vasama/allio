#include <allio/linux/detail/io_uring/byte_io.hpp>

#include <allio/impl/linux/byte_io.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <vsm/numeric.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = io_uring_multiplexer;
using H = native_handle<platform_object_t> const;
using C = io_uring_multiplexer::connector_type const;
using S = io_uring_byte_io_state_2;

template<uint8_t Opcode, typename Arguments>
static io_result<size_t> _submit(
	M& m,
	H& h,
	C& c,
	S& s,
	Arguments const& a,
	io_handler<M>& handler)
{
	off_t file_offset = -1;

	if constexpr (requires { a.offset; })
	{
		vsm_try_assign(file_offset, vsm::try_truncate<off_t>(
			a.offset,
			allio_error(error::file_offset_out_of_range)));
	}

	vsm_try_void(check_io_vectors_size(a.buffers));

	io_extension_allocator extension = initialize_extension(s);
	vsm_try(io_vectors, get_io_vectors(a.buffers, extension));

	io_uring_record_context ctx(m, a.deadline);
	auto const [fd, fd_flags] = ctx.get_fd(c, h.platform_handle);

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = Opcode,
		.flags = fd_flags,
		.fd = fd,
		.off = static_cast<uint64_t>(file_offset),
		.addr = reinterpret_cast<uintptr_t>(io_vectors.data()),
		.len = vsm::truncating(io_vectors.size()),
		.user_data = ctx.get_user_data(s),
	};

	if (a.deadline != deadline::never())
	{
		vsm_try_void(ctx.link_timeout(s.timeout.set(a.deadline)));
	}

	s.set_handler(handler);
	ctx.commit();
	extension.release();

	return vsm::unexpected(io_notify_status::submitted);
}

template<typename Arguments>
static io_result<size_t> _notify(
	M& m,
	H& h,
	C& c,
	S& s,
	Arguments const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	io_extension_allocator const extension = acquire_extension(s);

	// This operation uses no io_slots.
	vsm_assert(status.slot == nullptr);

	if (status.result < 0)
	{
		return vsm::unexpected(allio_error(static_cast<system_error>(-status.result)));
	}

	if (status.result == 0 && !io_buffers_is_empty(a.buffers))
	{
		return vsm::unexpected(allio_error(error::end_of_stream));
	}

	return static_cast<size_t>(status.result);
}


template<typename T>
static constexpr uint8_t select_opcode = 0;

template<vsm::any_of<byte_io::stream_read_t, byte_io::random_read_t> T>
static constexpr uint8_t select_opcode<T> = IORING_OP_READV;

template<vsm::any_of<byte_io::stream_write_t, byte_io::random_write_t> T>
static constexpr uint8_t select_opcode<T> = IORING_OP_WRITEV;

template<typename Operation>
io_result<size_t> io_uring_byte_io_state_1<Operation>::submit(
	M& m,
	H& h,
	C& c,
	S& s,
	A const& a,
	io_handler<M>& handler)
{
	return _submit<select_opcode<Operation>>(m, h, c, s, a, handler);
}

template<typename Operation>
io_result<size_t> io_uring_byte_io_state_1<Operation>::notify(
	M& m,
	H& h,
	C& c,
	S& s,
	A const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	return _notify(m, h, c, s, a, handler, status);
}

void io_uring_byte_io_state_2::cancel(M& m, H const&, C const&, S& s)
{
	(void)m.cancel_io(s);
}


template struct io_uring_byte_io_state_1<byte_io::stream_read_t>;
template struct io_uring_byte_io_state_1<byte_io::stream_write_t>;
template struct io_uring_byte_io_state_1<byte_io::random_read_t>;
template struct io_uring_byte_io_state_1<byte_io::random_write_t>;
