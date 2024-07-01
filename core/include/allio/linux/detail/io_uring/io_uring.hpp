#pragma once

#include <linux/io_uring.h>

namespace allio::detail {
inline namespace io_uring_constants {

#ifndef IORING_SETUP_SQE128
inline constexpr auto IORING_SETUP_SQE128           = 1U << 10;
#endif

#ifndef IORING_SETUP_CQE32
inline constexpr auto IORING_SETUP_CQE32            = 1U << 11;
#endif

#ifndef IORING_FEAT_CQE_SKIP
inline constexpr auto IORING_FEAT_CQE_SKIP          = 1U << 11;
#endif

#ifndef IOSQE_CQE_SKIP_SUCCESS
inline constexpr auto IOSQE_CQE_SKIP_SUCCESS        = 1U << 6;
#endif

} // inline namespace io_uring_constants
} // namespace allio::detail
