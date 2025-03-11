#include <allio/win32/detail/iocp/raw_listen_socket.hpp>

#include <allio/impl/io_extension.hpp>
#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/iocp/raw_socket.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/wsa.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/numeric.hpp>
#include <vsm/tag_ptr.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using M = iocp_multiplexer;
using H = native_handle<raw_listen_socket_t>;
using C = async_connector_t<M, raw_listen_socket_t>;

using socket_handle_type = basic_attached_handle<raw_socket_t, basic_multiplexer_handle<M>>;


using listen_t = raw_listen_socket_t::listen_t;
using listen_s = async_operation_t<M, raw_listen_socket_t, listen_t>;
using listen_a = io_parameters_t<raw_listen_socket_t, listen_t>;

io_result<void> listen_s::submit(M& m, H& h, C& c, listen_s& s, listen_a const& a, io_handler<M>&)
{
	if (vsm::any_flags(a.flags, io_flags::create_synchronous))
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	posix::socket_address_storage address_storage;
	vsm_try(addr, posix::get_socket_address(a.endpoint, address_storage));
	vsm_try(protocol, posix::choose_protocol(addr.addr->sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr->sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	vsm_try_void(posix::socket_listen(
		socket.get(),
		addr,
		a.backlog));

	vsm_try_void(m.attach_platform_handle(posix::wrap_socket(socket.get()), c));

	h.flags = object_t::flags::not_null | posix::set_address_family(addr.addr->sa_family) | flags;
	h.platform_handle = posix::wrap_socket(socket.release());

	return {};
}

io_result<void> listen_s::notify(
	M&,
	H&,
	C&,
	listen_s&,
	listen_a const&,
	io_handler<M>&,
	M::io_status_type)
{
	vsm_unreachable();
}

void listen_s::cancel(M&, H const&, C const&, listen_s&)
{
}


using accept_t = raw_listen_socket_t::accept_t;
using accept_s = async_operation_t<M, raw_listen_socket_t, accept_t>;
using accept_a = io_parameters_t<raw_listen_socket_t, accept_t>;

#if 0
class accept_extension : public io_extension_base
{
	static constexpr size_t address_padding_size = 16;

	struct storage_type
	{
		void* user_storage;
		alignas(posix::socket_address_union) unsigned char storage[];

		explicit storage_type(void* const user_storage)
			: user_storage(user_storage)
		{
		}

		[[nodiscard]] platform_endpoint_view copy_remote_addr() const
		{
			vsm_assert(user_storage != nullptr); //PRECONDITION

			sockaddr const* local_addr = reinterpret_cast<sockaddr const*>(storage);
			size_t const addr_size = posix::get_max_socket_address_size(local_addr->sa_family);
			size_t const remote_offset = addr_size + address_padding_size;
			std::memcpy(user_storage, storage + remote_offset, addr_size);
			return platform_endpoint_view(user_storage, addr_size);
		}
	};

	using ptr_type = vsm::variant_ptr<sockaddr, storage_type>;

public:
	using io_extension_base::io_extension_base;

	~accept_extension()
	{
		if (m_extension != nullptr)
		{
			auto const ptr = std::bit_cast<ptr_type>(m_extension->extension);
			if (storage_type* const storage = vsm::get_if<storage_type>(ptr))
			{
				delete_object(storage);
			}
		}
	}


	using storage_pair = std::pair<void*, size_t>;

	[[nodiscard]] vsm::result<storage_pair> initialize(
		size_t const max_address_size,
		any_endpoint_storage_provider storage_provider) const
	{
		vsm_assert(m_extension != nullptr); //PRECONDITION
		vsm_assert(m_extension->extension == nullptr); //PRECONDITION

		size_t const addr_storage_size = max_address_size + address_padding_size;
		size_t const storage_pair_size = 2 * addr_storage_size;

		void* user_storage = nullptr;

		if (storage_provider)
		{
			auto const r = storage_provider.get_storage(
				storage_pair_size,
				std::align_val_t(alignof(posix::socket_address_union)));

			if (r)
			{
				auto* const ptr = reinterpret_cast<sockaddr*>(r->storage);
				m_extension->extension = std::bit_cast<void*>(ptr_type(ptr));
				return storage_pair{ ptr, addr_storage_size };
			}

			vsm_try_assign(user_storage, storage_provider.get_storage(
				max_address_size,
				std::align_val_t(alignof(posix::socket_address_union))));
		}

		vsm_try(storage, detail::allocate_unique(storage_pair_size));
		storage_type* const ptr = ::new (storage.release()) storage_type(user_storage);
		m_extension->extension = std::bit_cast<void*>(ptr_type(ptr));

		return storage_pair{ ptr->storage, addr_storage_size };
	}

	[[nodiscard]] any_endpoint_view get_endpoint() const
	{
		vsm_assert(m_extension != nullptr); //PRECONDITION
		vsm_assert(m_extension->extension != nullptr); //PRECONDITION

		auto const variant = std::bit_cast<ptr_type>(m_extension->extension);
		if (storage_type* const storage = vsm::get_if<storage_type>(variant))
		{
			if (storage->user_storage != nullptr)
			{
				return storage->copy_remote_addr();
			}
			else
			{
				return null_endpoint;
			}
		}

		int const address_family = vsm::get_if<sockaddr>(variant)->sa_family;
		size_t const max_address_size = posix::get_max_socket_address_size(address_family);
		return platform_endpoint_view(variant.get(), max_address_size);
	}
};
#endif

namespace {

struct accept_address_storage
{
	void* user_storage;
	wsa_accept_address_storage addr_storage;
};
using accept_extension = io_extension_object<accept_address_storage>;

} // namespace

static io_result<socket_handle_type> handle_accept_completion(
	M& m,
	H const& h,
	accept_s& s,
	accept_a const& a,
	accept_extension const& extension)
{
	socket_handle_type::connector_type c;
	vsm_try_void(m.attach_platform_handle(s.socket.get(), c));

	int const address_family = posix::get_address_family(h.flags);
	size_t const max_address_size = posix::get_max_socket_address_size(address_family);

	if (a.endpoint && a.endpoint.is_platform_endpoint() && extension.has_object())
	{
		std::memcpy(extension->user_storage, &extension->addr_storage, max_address_size);
	}

	return vsm_lazy(socket_handle_type(
		adopt_handle,
		m,
		native_handle<raw_socket_t>
		{
			native_handle<platform_object_t>
			{
				native_handle<object_t>
				{
					object_t::flags::not_null | s.socket_flags,
				},
				s.socket.release(),
			},
		},
		vsm_move(c)));
}

io_result<socket_handle_type> accept_s::submit(
	M& m,
	H const& h,
	C const&,
	accept_s& s,
	accept_a const& a,
	io_handler<M>& handler)
{
	if (vsm::any_flags(a.flags, io_flags::create_synchronous))
	{
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	SOCKET const listen_socket = posix::unwrap_socket(h.platform_handle);

	int const address_family = posix::get_address_family(h.flags);
	size_t const max_address_size = posix::get_max_socket_address_size(address_family);
	size_t const min_address_storage_size = max_address_size + 16;
	vsm_assert(min_address_storage_size <= sizeof(wsa_accept_address_storage));

	accept_extension extension = initialize_extension(s);
	void* address_storage = nullptr;

	if (a.endpoint)
	{
		if (a.endpoint.is_platform_endpoint())
		{
			vsm_try(storage, a.endpoint.resize(
				max_address_size,
				min_address_storage_size,
				std::align_val_t(alignof(posix::socket_address_union))));

			if (storage.size >= min_address_storage_size)
			{
				address_storage = storage;
			}
			else
			{
				vsm_try(extension_storage, extension.emplace_default());
				extension_storage->user_storage = storage;
				address_storage = &extension_storage->addr_storage;
			}
		}
		else if (a.endpoint.kind() != posix::get_address_kind(address_family))
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}
		else
		{
			//TODO: Implement typed endpoint buffer usage.
			return vsm::unexpected(allio_error(error::unsupported_operation));
		}
	}

	vsm_try_bind((socket, flags), posix::create_socket(
		address_family,
		SOCK_STREAM,
		*posix::choose_protocol(address_family, SOCK_STREAM),
		a.flags));

	s.socket = unique_wrapped_socket(posix::wrap_socket(socket.release()));
	s.socket_flags = flags | posix::set_address_family(address_family);

	OVERLAPPED& overlapped = *s.overlapped;
	overlapped.Pointer = nullptr;
	overlapped.hEvent = NULL;

	s.overlapped.bind(handler);

	// If using a multithreaded completion port, after this call another thread will race to
	// complete this operation.
	vsm_try(already_completed, submit_socket_io(m, h, [&]() -> DWORD
	{
		return wsa_accept_ex(
			listen_socket,
			posix::unwrap_socket(s.socket.get()),
			address_storage,
			vsm::truncating(min_address_storage_size),
			&overlapped);
	}));

#if 0
		DWORD transferred = static_cast<DWORD>(-1);
		bool const result = win32::AcceptEx(
			listen_socket,
			posix::unwrap_socket(s.socket.get()),
			address_storage,
			/* dwReceiveDataLength: */ 0,
			/* dwLocalAddressLength: */ 0,
			vsm::truncating(min_address_storage_size),
			&transferred,
			&overlapped);

		if (!result)
		{
			return static_cast<DWORD>(WSAGetLastError());
		}

		vsm_assert(transferred == 0);
		return 0;
#endif

	if (already_completed)
	{
		return handle_accept_completion(m, h, s, a, extension);
	}

	extension.release();
	return vsm::unexpected(io_notify_status::submitted);
}

io_result<socket_handle_type> accept_s::notify(
	M& m,
	H const& h,
	C const&,
	accept_s& s,
	accept_a const& a,
	io_handler<M>& handler,
	M::io_status_type const status)
{
	accept_extension const extension = acquire_extension(s);

	vsm_assert(&status.slot == &s.overlapped);

	if (!NT_SUCCESS(status.status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status.status)));
	}

	return handle_accept_completion(m, h, s, a, extension);
}

void accept_s::cancel(M&, H const& h, C const&, S& s)
{
	cancel_socket_io(posix::unwrap_socket(h.platform_handle), *s.overlapped);
}
