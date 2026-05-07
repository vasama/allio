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

// TODO: Implement greedy I/O.

template<typename Result, uint8_t Opcode, typename Arguments>
static io_result<Result> _submit(
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
		.user_data = ctx.encode_user_data(s),
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

template<typename Result, typename Arguments>
static io_result<Result> _notify(
	M& m,
	H& h,
	C& c,
	S& s,
	Arguments const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	io_extension_allocator const extension = acquire_extension(s);

	if (status.result < 0)
	{
		return vsm::unexpected(allio_error(static_cast<system_error>(-status.result)));
	}

	if constexpr (std::is_same_v<Result, read_result>)
	{
		if (status.result == 0 && !io_buffers_is_empty(a.buffers))
		{
			return end_of_stream;
		}
	}

	return static_cast<size_t>(status.result);
}


namespace {

template<typename T>
struct select_opcode;

template<vsm::any_of<byte_io::stream_read_t, byte_io::random_read_t> T>
struct select_opcode<T>
{
	static constexpr uint8_t value = IORING_OP_READV;
};

template<vsm::any_of<byte_io::stream_write_t, byte_io::random_write_t> T>
struct select_opcode<T>
{
	static constexpr uint8_t value = IORING_OP_WRITEV;
};

} // namespace

template<typename Operation>
io_result<typename Operation::result_type> io_uring_byte_io_state_1<Operation>::submit(
	M& m,
	H& h,
	C& c,
	S& s,
	A const& a,
	io_handler<M>& handler)
{
	return _submit<R, select_opcode<Operation>::value>(m, h, c, s, a, handler);
}

template<typename Operation>
io_result<typename Operation::result_type> io_uring_byte_io_state_1<Operation>::notify(
	M& m,
	H& h,
	C& c,
	S& s,
	A const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	return _notify<R>(m, h, c, s, a, handler, status);
}

void io_uring_byte_io_state_2::cancel(M& m, H const&, C const&, S& s, A const&, io_handler<M> const&)
{
	(void)m.cancel_io(s);
}


template struct io_uring_byte_io_state_1<byte_io::stream_read_t>;
template struct io_uring_byte_io_state_1<byte_io::stream_write_t>;
template struct io_uring_byte_io_state_1<byte_io::random_read_t>;
template struct io_uring_byte_io_state_1<byte_io::random_write_t>;
