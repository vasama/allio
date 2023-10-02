#pragma once

#include <linux/io_uring.h>

#ifndef IORING_SETUP_SQE128
#	define IORING_SETUP_SQE128                  (1U << 10)
#endif

#ifndef IORING_SETUP_CQE32
#	define IORING_SETUP_CQE32                   (1U << 11)
#endif

#ifndef IOSQE_CQE_SKIP_SUCCESS
#	define IOSQE_CQE_SKIP_SUCCESS               (1U << 6)
#endif
