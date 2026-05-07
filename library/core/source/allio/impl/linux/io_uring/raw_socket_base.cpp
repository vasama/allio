#include <allio/linux/detail/io_uring/raw_socket.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/linux/io_uring_record_context.hpp>

#include <vsm/numeric.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = io_uring_multiplexer;
using H = native_handle<platform_object_t>;
using C = async_connector_t<M, platform_object_t>;

using get_address_s = io_uring_raw_socket_get_address_1;
using get_address_a = io_parameters_t<raw_socket_t, get_address_t>;

io_result<void> get_address_s::submit(
	M& m,
	H const& h,
	C const& c,
	get_address_s& s,
	get_address_a const& a,
	io_handler<M>& handler)
{
	static_assert(std::is_same_v<decltype(s.addr_size), socklen_t>);

	size_t const max_address_size =
		posix::get_max_socket_address_size(posix::get_address_family(h.flags));

	vsm_try(address_storage, a.endpoint.resize(
		max_address_size,
		max_address_size,
		std::align_val_t(alignof(posix::socket_address_union))));

	s.addr_size = vsm::truncating(address_storage.size);

	io_uring_record_context ctx(m, deadline::never());

	vsm_try_ptr(sqe, ctx.push());

	sqe =
	{
		.opcode = IORING_OP_URING_CMD,
		.fd = unwrap_handle(h.platform_handle),
		.cmd_op = SOCKET_URING_OP_GETSOCKNAME,
		.addr = reinterpret_cast<uintptr_t>(address_storage.storage),
		.user_data = ctx.encode_user_data(s),
		.addr3 = reinterpret_cast<uintptr_t>(&s.addr_size),
	};

	s.set_handler(handler);
	ctx.commit();

	return vsm::unexpected(io_notify_status::submitted);
}

io_result<void> get_address_s::notify(
	M& m,
	H const& h,
	C const& c,
	get_address_s& s,
	get_address_a const& a,
	io_handler<M>&,
	M::io_status_type const status)
{
	if (status.result < 0)
	{
		return vsm::unexpected(allio_error(static_cast<system_error>(-status.result)));
	}

	return {};
}

void get_address_s::cancel(M& m, H const&, C const&, S& s, A const&, io_handler<M> const&)
{
	(void)m.cancel_io(s);
}
