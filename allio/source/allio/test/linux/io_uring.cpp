#include <allio/impl/linux/kernel/io_uring.hpp>

#include <allio/event.hpp>
#include <allio/linux/detail/unique_mmap.hpp>
#include <allio/linux/platform.hpp>

#include <vsm/atomic.hpp>

#include <catch2/catch_all.hpp>

#include <linux/time_types.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

template<typename T>
static vsm::result<unique_mmap<T>> mmap(int const fd, uint64_t const offset, size_t const size)
{
	void* const addr = ::mmap(
		nullptr,
		size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_POPULATE,
		fd,
		offset);

	if (addr == MAP_FAILED)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(unique_mmap<T>(reinterpret_cast<T*>(addr), mmap_deleter(size)));
}

static vsm::result<std::pair<unique_fd, unique_fd>> create_pipe_pair()
{
	int fds[2];
	if (pipe(fds) == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	return vsm_lazy(std::pair<unique_fd, unique_fd>(fds[0], fds[1]));
}


TEST_CASE("io_uring", "[io_uring]")
{
	io_uring_params setup = {};
	auto const io_uring = io_uring_setup(32, setup).value();

	auto const sq_mmap = mmap<std::byte>(
		io_uring.get(),
		IORING_OFF_SQ_RING,
		setup.sq_off.array + setup.sq_entries * sizeof(uint32_t)).value();
	auto const sq_ring = std::span(
		reinterpret_cast<uint32_t*>(sq_mmap.get() + setup.sq_off.array),
		setup.sq_entries);

	auto const cq_mmap = mmap<std::byte>(
		io_uring.get(),
		IORING_OFF_CQ_RING,
		setup.cq_off.cqes + setup.cq_entries * sizeof(io_uring_cqe)).value();
	auto const cq_ring = std::span(
		reinterpret_cast<io_uring_cqe*>(cq_mmap.get() + setup.cq_off.cqes),
		setup.cq_entries);

	auto const sqes_mmap = mmap<io_uring_sqe>(
		io_uring.get(),
		IORING_OFF_SQES,
		setup.sq_entries * sizeof(io_uring_sqe)).value();
	auto const sqes = std::span(
		sqes_mmap.get(),
		setup.sq_entries);

	auto const get_k = [&](unique_mmap<std::byte> const& mmap, size_t const offset)
	{
		return vsm::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(mmap.get() + offset));
	};

	auto const k_sq_produce = get_k(sq_mmap, setup.sq_off.tail);
	auto const k_cq_produce = get_k(cq_mmap, setup.cq_off.tail);

	uint32_t sqe_offset = 0;
	uint32_t sq_produce = k_sq_produce.load(std::memory_order_relaxed);
	uint32_t cq_consume = get_k(cq_mmap, setup.cq_off.head).load(std::memory_order_relaxed);
	uint32_t cq_produce = cq_consume;

	auto const push_sqe = [&](io_uring_sqe const& sqe)
	{
		sqes[sqe_offset] = sqe;
		sq_ring[sq_produce++] = sqe_offset++;
	};

	auto const enter = [&](uint32_t min_complete)
	{
		if (uint32_t to_submit = sq_produce - k_sq_produce.load(std::memory_order_acquire))
		{
			k_sq_produce.store(sq_produce, std::memory_order_release);

			while (to_submit != 0)
			{
				to_submit -= io_uring_enter(
					io_uring.get(),
					to_submit,
					/* min_complete: */ 0,
					/* flags: */ 0,
					/* arg: */ nullptr).value();
			}
		}

		if (min_complete != 0)
		{
			(void)io_uring_enter(
				io_uring.get(),
				/* to_submit: */ 0,
				min_complete,
				/* flags: */ 0,
				/* arg: */ nullptr).value();

			cq_produce = k_cq_produce.load(std::memory_order_acquire);
		}
	};

	auto const find_cqe = [&](uintptr_t const user_data) -> io_uring_cqe const*
	{
		for (uint32_t i = cq_consume; i != cq_produce; ++i)
		{
			io_uring_cqe const& cqe = cq_ring[i % cq_ring.size()];

			if (cqe.user_data == user_data)
			{
				return &cqe;
			}
		}

		return nullptr;
	};

	auto const reap_cqe = [&](
		uintptr_t const user_data,
		int32_t const result,
		std::optional<uint32_t> const flags = std::nullopt)
	{
		auto const cqe = find_cqe(user_data);
		REQUIRE(cqe != nullptr);
		REQUIRE(cqe->res == result);
		if (flags)
		{
			REQUIRE(cqe->flags == *flags);
		}
	};

	auto const poll_io_uring = [&](short const events) -> bool
	{
		pollfd pfd =
		{
			.fd = io_uring.get(),
			.events = events,
		};

		int const count = poll(
			&pfd,
			/* nfds: */ 1,
			/* timeout: */ 0);

		REQUIRE(count >= 0);
		return count == 1;
	};


	auto const pipe_pair = create_pipe_pair().value();
	auto const r_fd = pipe_pair.first.get();
	auto const w_fd = pipe_pair.second.get();


	SECTION("NOP -> OK")
	{
		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.user_data = 1,
		});

		enter(1);

		reap_cqe(1, 0);
	}

	SECTION("NOP-NOP -> OK,OK")
	{
		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.flags = IOSQE_IO_LINK,
			.user_data = 2,
		});

		enter(2);

		reap_cqe(1, 0);
		reap_cqe(2, 0);
	}

	SECTION("ready POLL -> OK")
	{
		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.fd = w_fd,
			.poll_events = POLLOUT,
			.user_data = 1,
		});

		enter(1);
		reap_cqe(1, POLLOUT);
	}

	SECTION("POLL -> OK when ready")
	{
		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.fd = r_fd,
			.poll_events = POLLIN,
			.user_data = 1,
		});

		enter(0);

		REQUIRE(find_cqe(1) == nullptr);
		REQUIRE(write(w_fd, "", 1) == 1);

		enter(1);

		reap_cqe(1, POLLIN);
	}

	SECTION("POLL|ASYNC_CANCEL -> ECANCELED,OK")
	{
		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.fd = r_fd,
			.poll_events = POLLIN,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_ASYNC_CANCEL,
			.addr = 1,
			.user_data = 2,
		});

		enter(2);

		reap_cqe(1, -ECANCELED);
		reap_cqe(2, 0);
	}

	SECTION("TIMEOUT -> ETIME")
	{
		__kernel_timespec ts = {};

		push_sqe(
		{
			.opcode = IORING_OP_TIMEOUT,
			.addr = reinterpret_cast<uintptr_t>(&ts),
			.len = 1,
			.user_data = 1,
		});

		enter(1);

		reap_cqe(1, -ETIME);
	}

	SECTION("NOP-LINK_TIMEOUT -> OK,ECANCELED")
	{
		__kernel_timespec ts = {};

		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.flags = IOSQE_IO_LINK,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.addr = reinterpret_cast<uintptr_t>(&ts),
			.len = 1,
			.user_data = 2,
		});

		enter(2);

		reap_cqe(1, 0);
		reap_cqe(2, -ECANCELED);
	}

	SECTION("ready POLL-LINK_TIMEOUT -> OK,ECANCELED")
	{
		__kernel_timespec ts = {};

		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.flags = IOSQE_IO_LINK,
			.fd = w_fd,
			.poll_events = POLLOUT,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.addr = reinterpret_cast<uintptr_t>(&ts),
			.len = 1,
			.user_data = 2,
		});

		enter(2);

		reap_cqe(1, POLLOUT);
		reap_cqe(2, -ECANCELED);
	}

	SECTION("POLL-LINK_TIMEOUT -> ECANCELED,ETIME")
	{
		__kernel_timespec ts = {};

		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.flags = IOSQE_IO_LINK,
			.fd = r_fd,
			.poll_events = POLLIN,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.addr = reinterpret_cast<uintptr_t>(&ts),
			.len = 1,
			.user_data = 2,
		});

		enter(2);

		reap_cqe(1, -ECANCELED);
		reap_cqe(2, -ETIME);
	}

	SECTION("NOP-LINK_TIMEOUT-NOP -> OK,ECANCELED,OK")
	{
		__kernel_timespec ts = {};

		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.flags = IOSQE_IO_LINK,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.flags = IOSQE_IO_LINK,
			.addr = reinterpret_cast<uintptr_t>(&ts),
			.len = 1,
			.user_data = 2,
		});
		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.user_data = 3,
		});

		enter(3);

		reap_cqe(1, 0);
		reap_cqe(2, -ECANCELED);
		reap_cqe(3, 0);
	}

	SECTION("POLL-LINK_TIMEOUT-NOP -> ECANCELED,ETIME,ECANCELED")
	{
		__kernel_timespec ts = {};

		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.flags = IOSQE_IO_LINK,
			.fd = r_fd,
			.poll_events = POLLIN,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.flags = IOSQE_IO_LINK,
			.addr = reinterpret_cast<uintptr_t>(&ts),
			.len = 1,
			.user_data = 2,
		});
		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.user_data = 3,
		});

		enter(3);

		reap_cqe(1, -ECANCELED);
		reap_cqe(2, -ETIME);
		reap_cqe(3, -ECANCELED);
	}

	SECTION("POLL-LINK_TIMEOUT+NOP -> ECANCELED,ETIME,ECANCELED")
	{
		__kernel_timespec ts = {};

		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.flags = IOSQE_IO_LINK,
			.fd = r_fd,
			.poll_events = POLLIN,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.flags = IOSQE_IO_HARDLINK,
			.addr = reinterpret_cast<uintptr_t>(&ts),
			.len = 1,
			.user_data = 2,
		});
		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.user_data = 3,
		});

		enter(3);

		reap_cqe(1, -ECANCELED);
		reap_cqe(2, -ETIME);
		reap_cqe(3, -ECANCELED);
	}

	SECTION("POLL-LINK_TIMEOUT|NOP -> ECANCELED,ETIME,OK")
	{
		__kernel_timespec ts = {};

		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.flags = IOSQE_IO_LINK,
			.fd = r_fd,
			.poll_events = POLLIN,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.addr = reinterpret_cast<uintptr_t>(&ts),
			.len = 1,
			.user_data = 2,
		});
		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.user_data = 3,
		});

		enter(3);

		reap_cqe(1, -ECANCELED);
		reap_cqe(2, -ETIME);
		reap_cqe(3, 0);
	}

	SECTION("POLL-LINK_TIMEOUT-NOP|ASYNC_CANCEL -> ECANCELED,ECANCELED,ECANCELED,OK")
	{
		__kernel_timespec ts = { 1000 };

		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.flags = IOSQE_IO_LINK,
			.fd = r_fd,
			.poll_events = POLLIN,
			.user_data = 1,
		});
		push_sqe(
		{
			.opcode = IORING_OP_LINK_TIMEOUT,
			.flags = IOSQE_IO_LINK,
			.addr = reinterpret_cast<uintptr_t>(&ts),
			.len = 1,
			.user_data = 2,
		});
		push_sqe(
		{
			.opcode = IORING_OP_NOP,
			.user_data = 3,
		});
		push_sqe(
		{
			.opcode = IORING_OP_ASYNC_CANCEL,
			.addr = 1,
			.user_data = 4,
		});

		enter(4);

		reap_cqe(1, -ECANCELED);
		reap_cqe(2, -ECANCELED);
		reap_cqe(3, -ECANCELED);
		reap_cqe(4, 0);
	}

	SECTION("io_uring can be polled for CQEs")
	{
		REQUIRE(!poll_io_uring(POLLIN));

		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.fd = r_fd,
			.poll_events = POLLIN,
		});

		enter(0);
		REQUIRE(!poll_io_uring(POLLIN));

		REQUIRE(write(w_fd, "", 1) == 1);
		REQUIRE(poll_io_uring(POLLIN));
	}

	SECTION("CQEs are posted on completion without entering.")
	{
		push_sqe(
		{
			.opcode = IORING_OP_POLL_ADD,
			.fd = r_fd,
			.poll_events = POLLIN,
		});

		enter(0);
		REQUIRE(cq_produce == k_cq_produce.load(std::memory_order_acquire));

		REQUIRE(write(w_fd, "", 1) == 1);
		REQUIRE(cq_produce != k_cq_produce.load(std::memory_order_acquire));
	}

	if (setup.features & IORING_FEAT_CQE_SKIP)
	{
		SECTION("POLL?-NOP|ASYNC_CANCEL -> ECANCELED,NONE,OK")
		{
			push_sqe(
			{
				.opcode = IORING_OP_POLL_ADD,
				.flags = IOSQE_IO_LINK | IOSQE_CQE_SKIP_SUCCESS,
				.fd = r_fd,
				.poll_events = POLLIN,
				.user_data = 1,
			});
			push_sqe(
			{
				.opcode = IORING_OP_NOP,
				.user_data = 2,
			});
			push_sqe(
			{
				.opcode = IORING_OP_ASYNC_CANCEL,
				.addr = 1,
				.user_data = 3,
			});

			enter(2);

			reap_cqe(1, -ECANCELED);
			REQUIRE(find_cqe(2) == nullptr);
			reap_cqe(2, 0);
		}

		SECTION("POLL?-LINK_TIMEOUT?-NOP|ASYNC_CANCEL -> ECANCELED,NONE,NONE,OK")
		{
			__kernel_timespec ts = { 1000 };

			push_sqe(
			{
				.opcode = IORING_OP_POLL_ADD,
				.flags = IOSQE_IO_LINK | IOSQE_CQE_SKIP_SUCCESS,
				.fd = r_fd,
				.poll_events = POLLIN,
				.user_data = 1,
			});
			push_sqe(
			{
				.opcode = IORING_OP_LINK_TIMEOUT,
				.flags = IOSQE_IO_LINK | IOSQE_CQE_SKIP_SUCCESS,
				.addr = reinterpret_cast<uintptr_t>(&ts),
				.len = 1,
				.user_data = 2,
			});
			push_sqe(
			{
				.opcode = IORING_OP_NOP,
				.user_data = 3,
			});
			push_sqe(
			{
				.opcode = IORING_OP_ASYNC_CANCEL,
				.addr = 1,
				.user_data = 4,
			});

			enter(2);

			reap_cqe(1, -ECANCELED);
			REQUIRE(find_cqe(2) == nullptr);
			REQUIRE(find_cqe(3) == nullptr);
			reap_cqe(4, 0);
		}
	}

	if (setup.features & IORING_FEAT_EXT_ARG)
	{
		SECTION("Enter with timeout completes successfully")
		{
			push_sqe(
			{
				.opcode = IORING_OP_POLL_ADD,
				.fd = r_fd,
				.poll_events = POLLIN,
			});

			enter(0);

			// Time out after 10 milliseconds.
			__kernel_timespec ts = { 0, 10'000'000 };
			io_uring_getevents_arg arg =
			{
				.ts = reinterpret_cast<uintptr_t>(&ts),
			};

			int const r = _io_uring_enter(
				io_uring.get(),
				/* to_submit: */ 0,
				/* min_complete: */ 1,
				IORING_ENTER_EXT_ARG,
				&arg,
				sizeof(arg));

			REQUIRE(r == 0);
		}
	}
}
