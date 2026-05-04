#pragma once

#include <linux/io_uring.h>

namespace allio::detail {
inline namespace io_uring_constants {

#ifndef IORING_SETUP_SQE128
inline constexpr auto IORING_SETUP_SQE128                       = 1U << 10;
inline constexpr auto IORING_SETUP_SQE128                       = 1U << 10;
#endif

#ifndef IORING_SETUP_CQE32
inline constexpr auto IORING_SETUP_CQE32                        = 1U << 11;
#endif

#ifndef IORING_FEAT_CQE_SKIP
inline constexpr auto IORING_FEAT_CQE_SKIP                      = 1U << 11;
#endif

#ifndef IOSQE_CQE_SKIP_SUCCESS
inline constexpr auto IOSQE_CQE_SKIP_SUCCESS                    = 1U << 6;
#endif

#ifndef IORING_REGISTER_SEND_MSG_RING
inline constexpr auto IORING_REGISTER_SEND_MSG_RING             = 31;
#endif

enum io_uring_socket_op
{
	SOCKET_URING_OP_SIOCINQ,
	SOCKET_URING_OP_SIOCOUTQ,
	SOCKET_URING_OP_GETSOCKOPT,
	SOCKET_URING_OP_SETSOCKOPT,
	SOCKET_URING_OP_TX_TIMESTAMP,
	SOCKET_URING_OP_GETSOCKNAME,
};

} // inline namespace io_uring_constants
} // namespace allio::detail
