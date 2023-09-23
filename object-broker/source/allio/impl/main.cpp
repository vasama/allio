#include <allio/process_handle.hpp>
#include <allio/win32/pipe_handle.hpp>

using namespace allio;
using namespace allio::win32;

namespace {

enum class request_kind : uint16_t
{
	open_client_pipe,
	open_server_pipe,

	propose_handle_transfer,
	receive_handle_transfer,
};

struct open_pipe
{
	uint32_t pipe_handle;
};

struct propose_handle_transfer
{
	uint32_t handle;
};

struct receive_handle_transfer
{
	uint64_t handle_write_address;
};

union request_data
{
	propose_handle_transfer propose_handle_transfer;
	receive_handle_transfer receive_handle_transfer;
};


class broker_server
{
	struct session
	{
		pipe_handle pipe;
		process_handle process;
	};

	struct connection
	{
		session* session_0;
		session* session_1;
	};

	vsm::result<void> do_propose_handle_transfer(session& session, propose_handle_transfer const& request)
	{

	}

	vsm::result<void> do_receive_handle_transfer(session& session, receive_handle_transfer const& request)
	{
		
	}

	ex::task<void> session_handler(session& session)
	{
		while (true)
		{
			request_kind kind;
			request_data data;

			vsm_co_try_void(co_await session.pipe.read_async(
			{
				as_read_buffer(&kind, 1),
				as_read_buffer(&data, 1),
			}));

			switch (kind)
			{
			case request_kind::create_connection:
				{

				}
				break;

			case request_kind::accept_connection:
				{

				}
				break;

			case request_kind::propose_handle_transfer:
				{
					do_propose_handle_transfer(session, request.propose_handle_transfer);
				}
				break;

			case request_kind::receive_handle_transfer:
				{
					do_propose_handle_transfer(session, request.receive_handle_transfer);
				}
				break;
			}
		}
	}

	ex::task<void> listener()
	{
		vsm_co_try(pipe, open_pipe(L"allio-object-broker"));
		vsm_co_try_void(pipe.connect_async());



		session(vsm_move(pipe));
	}
};

} // namespace

static ex::task<void> session(pipe_handle const pipe)
{
	vsm_co_try(process, pipe.open_process());

	while (true)
	{
		request_kind kind;
		request_data request;

		vsm_co_try_void(co_await pipe.read_async(
		{
			as_read_buffer(&kind, 1),
			as_read_buffer(&request, 1),
		}));

		switch (kind)
		{
		case request_kind::propose_handle_transfer:
			{
				request.propose_handle_transfer;
			}
			break;
		}
	}
}

static ex::task<void> main_()
{
	while (true)
	{
		vsm_co_try(pipe, open_pipe(L"allio-object-broker"));
		vsm_co_try_void(pipe.connect_async());
		session(vsm_move(pipe));
	}
}
