#pragma once

#include <allio/any_path.hpp>
#include <allio/detail/aligned_storage_provider.hpp>
#include <allio/detail/mutable_buffer.hpp>
#include <allio/detail/parameters.hpp>

#include <vsm/array.hpp>
#include <vsm/assert.h>
#include <vsm/exceptions.hpp>
#include <vsm/int128.hpp>
#include <vsm/result.hpp>
#include <vsm/standard/bit.hpp>
#include <vsm/standard/memory.hpp>
#include <vsm/utility.hpp>

#include <bit>
#include <charconv>
#include <compare>
#include <concepts>
#include <optional>
#include <string_view>

#include <cstdint>

namespace allio {

template<std::integral T>
[[nodiscard]] constexpr T network_byte_order(T const value)
{
	if constexpr (std::endian::native == std::endian::little)
	{
		return vsm::byteswap(value);
	}
	else if constexpr (std::endian::native == std::endian::big)
	{
		return value;
	}
	else
	{
		static_assert(sizeof(T) == 0);
	}
}


enum class network_address_kind : uint8_t
{
	null,
	local, //TODO: Rename to file or path?
	ipv4,
	ipv6,
};


struct null_endpoint_t {};
inline constexpr null_endpoint_t null_endpoint = {};

//TODO: Rename to local_endpoint and use in any_endpoint_view, or get rid of.
class local_address
{
	path_view m_path;

public:
	local_address() = default;

	explicit constexpr local_address(path_view const path)
		: m_path(path)
	{
	}

	[[nodiscard]] constexpr path_view path() const
	{
		return m_path;
	}
};


using network_port_t = uint16_t;

// Note that ipv4_address always represents addresses using host byte order. This means that for
// example the loopback address 127.0.0.1 is unambiguously represented as 0x7f'00'00'01. Some use
// cases outside of this library may require explicit conversion to network byte order.
class ipv4_address
{
public:
	using uint_type = uint32_t;

private:
	uint_type m_addr;

public:
	ipv4_address() = default;

	explicit constexpr ipv4_address(uint_type const integer)
		: m_addr(integer)
	{
	}

	[[nodiscard]] constexpr uint_type integer() const
	{
		return m_addr;
	}


	static ipv4_address const unspecified;
	static ipv4_address const localhost;

	[[nodiscard]] static vsm::result<ipv4_address> parse(std::string_view string);


	[[nodiscard]] friend auto operator<=>(ipv4_address const&, ipv4_address const&) = default;
};

inline constexpr ipv4_address ipv4_address::unspecified = ipv4_address(0);
inline constexpr ipv4_address ipv4_address::localhost = ipv4_address(0x7f'00'00'01);

struct ipv4_endpoint
{
	ipv4_address address;
	network_port_t port;


	[[nodiscard]] static vsm::result<ipv4_endpoint> parse(std::string_view string);

	[[nodiscard]] friend auto operator<=>(ipv4_endpoint const&, ipv4_endpoint const&) = default;
};


using ipv6_zone_t = uint32_t;

class ipv6_address
{
	using addr_type = vsm::array<uint32_t, 4>;

public:
	using uint_type = vsm::uint128_t;

private:
	addr_type m_addr;

public:
	ipv6_address() = default;

	explicit constexpr ipv6_address(uint_type const integer)
		: m_addr(std::bit_cast<addr_type>(integer))
	{
	}

	[[nodiscard]] constexpr uint_type integer() const
	{
		return std::bit_cast<uint_type>(m_addr);
	}


	static ipv6_address const unspecified;
	static ipv6_address const localhost;

	[[nodiscard]] static vsm::result<ipv6_address> parse(std::string_view string);


	[[nodiscard]] friend auto operator<=>(ipv6_address const&, ipv6_address const&) = default;
};

inline constexpr ipv6_address ipv6_address::unspecified = ipv6_address(0);
inline constexpr ipv6_address ipv6_address::localhost = ipv6_address(1);

struct ipv6_endpoint
{
	ipv6_address address;
	network_port_t port;
	ipv6_zone_t zone;


	[[nodiscard]] static vsm::result<ipv6_endpoint> parse(std::string_view string);

	[[nodiscard]] friend auto operator<=>(ipv6_endpoint const&, ipv6_endpoint const&) = default;
};


class network_endpoint
{
	network_address_kind m_kind;
	union
	{
		null_endpoint_t m_null;
		ipv4_endpoint m_ipv4;
		ipv6_endpoint m_ipv6;
	};

public:
	constexpr network_endpoint()
		: m_kind(network_address_kind::null)
		, m_null{}
	{
	}

	constexpr network_endpoint(null_endpoint_t)
		: m_kind(network_address_kind::null)
		, m_null{}
	{
	}

	constexpr network_endpoint(ipv4_endpoint const& endpoint)
		: m_kind(network_address_kind::ipv4)
		, m_ipv4(endpoint)
	{
	}

	constexpr network_endpoint(ipv6_endpoint const& endpoint)
		: m_kind(network_address_kind::ipv6)
		, m_ipv6(endpoint)
	{
	}

	[[nodiscard]] constexpr network_address_kind kind() const
	{
		return m_kind;
	}

	[[nodiscard]] constexpr bool is_null() const
	{
		return m_kind == network_address_kind::null;
	}

	[[nodiscard]] constexpr ipv4_endpoint const& ipv4() const
	{
		vsm_assert(m_kind == network_address_kind::ipv4);
		return m_ipv4;
	}

	[[nodiscard]] constexpr ipv6_endpoint const& ipv6() const
	{
		vsm_assert(m_kind == network_address_kind::ipv6);
		return m_ipv6;
	}

	[[nodiscard]] decltype(auto) visit(auto&& visitor) const
	{
		switch (m_kind)
		{
		case network_address_kind::null:
			return vsm_forward(visitor)(null_endpoint_t());

		case network_address_kind::ipv4:
			return vsm_forward(visitor)(m_ipv4);

		case network_address_kind::ipv6:
			return vsm_forward(visitor)(m_ipv6);
		}
	}
};


inline constexpr size_t platform_endpoint_alignment = 4;

class platform_endpoint_view
{
	void const* m_data;
	size_t m_size;

public:
	template<vsm::no_cvref_of<platform_endpoint_view> Range>
		requires
			std::ranges::contiguous_range<Range> &&
			vsm::byte_type<vsm::remove_cv_t<std::ranges::range_value_t<Range>>>
	explicit platform_endpoint_view(Range const& range)
		: platform_endpoint_view(std::ranges::data(range), std::ranges::size(range))
	{
	}

	explicit platform_endpoint_view(void const* const data, size_t const size)
		: m_data(data)
		, m_size(size)
	{
		vsm_assert(vsm::is_sufficiently_aligned<platform_endpoint_alignment>(data)); //PRECONDITION
	}

	[[nodiscard]] void const* data() const
	{
		return m_data;
	}

	[[nodiscard]] size_t size() const
	{
		return m_size;
	}
};

class any_endpoint_view
{
	struct local_t
	{
		explicit local_t() = default;
	};

	static constexpr uint32_t ctrl_bits = 32;

	static constexpr uint32_t type_bits = 4;
	static constexpr uint32_t type_mask = (static_cast<uint32_t>(1) << type_bits) - 1;

	static constexpr uint32_t size_bits = ctrl_bits - type_bits;
	static constexpr uint32_t size_mask = (static_cast<uint32_t>(1) << size_bits) - 1;

	static constexpr uint32_t error_type                        = 0;
	static constexpr uint32_t platform_type                     = 1;
	static constexpr uint32_t generic_type_offset               = 2;

	void const* m_data;
	uint32_t m_ctrl;


	static consteval uint32_t make_kind_type(network_address_kind const kind)
	{
		return static_cast<uint32_t>(kind) + generic_type_offset;
	}

	static consteval uint32_t make_kind_ctrl(uint32_t const type)
	{
		return type << size_bits;
	}

	static consteval uint32_t make_kind_ctrl(network_address_kind const kind)
	{
		return make_kind_ctrl(make_kind_type(kind));
	}

public:
	any_endpoint_view()
		: any_endpoint_view(nullptr, 0, make_kind_ctrl(error_type))
	{
	}

	any_endpoint_view(null_endpoint_t)
		: any_endpoint_view(nullptr, 0, make_kind_ctrl(network_address_kind::null))
	{
	}

	template<detail::_any_path Path>
	any_endpoint_view(Path const& path)
		: any_endpoint_view(local_t(), get_path_string(path))
	{
		static_assert(
			std::is_same_v<typename Path::value_type, char>,
			"Automatic transcoding of local addresses is not currently supported.");
	}

	any_endpoint_view(ipv4_endpoint const& endpoint)
		: any_endpoint_view(
			&endpoint,
			sizeof(endpoint),
			make_kind_ctrl(network_address_kind::ipv4))
	{
	}

	any_endpoint_view(ipv6_endpoint const& endpoint)
		: any_endpoint_view(
			&endpoint,
			sizeof(endpoint),
			make_kind_ctrl(network_address_kind::ipv6))
	{
	}

	any_endpoint_view(platform_endpoint_view const view)
		: any_endpoint_view(
			view.data(),
			view.size(),
			make_kind_ctrl(platform_type))
	{
	}

	[[nodiscard]] bool is_generic_endpoint() const
	{
		return m_ctrl >> size_bits > generic_type_offset;
	}

	[[nodiscard]] network_address_kind get_generic_kind() const
	{
		vsm_assert(is_generic_endpoint()); //PRECONDITION
		return static_cast<network_address_kind>((m_ctrl >> size_bits) - generic_type_offset);
	}

	template<std::same_as<path_view>>
	[[nodiscard]] path_view get_generic_endpoint() const
	{
		vsm_assert(get_generic_kind() == network_address_kind::local); //PRECONDITION
		return path_view(static_cast<char const*>(m_data), m_ctrl & size_mask);
	}

	template<std::same_as<ipv4_endpoint>>
	[[nodiscard]] ipv4_endpoint const& get_generic_endpoint() const
	{
		vsm_assert(get_generic_kind() == network_address_kind::ipv4); //PRECONDITION
		return *static_cast<ipv4_endpoint const*>(m_data);
	}

	template<std::same_as<ipv6_endpoint>>
	[[nodiscard]] ipv6_endpoint const& get_generic_endpoint() const
	{
		vsm_assert(get_generic_kind() == network_address_kind::ipv6); //PRECONDITION
		return *static_cast<ipv6_endpoint const*>(m_data);
	}

	[[nodiscard]] bool is_platform_endpoint() const
	{
		return m_ctrl >> size_bits == platform_type;
	}

	[[nodiscard]] platform_endpoint_view get_platform_endpoint() const
	{
		vsm_assert(is_platform_endpoint()); //PRECONDITION
		return platform_endpoint_view(
			static_cast<std::byte const*>(m_data),
			m_ctrl & size_mask);
	}

	[[nodiscard]] decltype(auto) visit(auto&& visitor) const
	{
		switch (m_ctrl >> size_bits)
		{
		case platform_type:
			return vsm_forward(visitor)(get_platform_endpoint());

		case make_kind_type(network_address_kind::null):
			return vsm_forward(visitor)(null_endpoint_t());

		case make_kind_type(network_address_kind::local):
			return vsm_forward(visitor)(get_generic_endpoint<path_view>());

		case make_kind_type(network_address_kind::ipv4):
			return vsm_forward(visitor)(get_generic_endpoint<ipv4_endpoint>());

		case make_kind_type(network_address_kind::ipv6):
			return vsm_forward(visitor)(get_generic_endpoint<ipv6_endpoint>());
		}

		//TODO: Rename the error tag to be more generic, move out of detail.
		return vsm_forward(visitor)(detail::string_length_out_of_range_t());
	}

private:
	explicit any_endpoint_view(
		void const* const data,
		size_t const size,
		uint32_t const ctrl)
		: m_data(data)
		, m_ctrl(size > size_mask ? 0 : ctrl | static_cast<uint32_t>(size))
	{
	}

	explicit any_endpoint_view(local_t, std::string_view const path)
		: any_endpoint_view(
			path.data(),
			path.size(),
			make_kind_ctrl(network_address_kind::local))
	{
	}
};


#if 0
template<detail::resizable_container Container>
	requires vsm::byte_type<typename Container::value_type>
class basic_platform_endpoint
{
	Container m_container;

public:

private:
	friend vsm::result<vsm::allocation> tag_invoke(
		detail::get_storage_t,
		basic_platform_endpoint& endpoint,
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment)
	{
		
	}
};

//TODO: Use custom default container type with small storage optimisation.
class platform_endpoint = basic_platform_endpoint<std::vector<unsigned char>>;
#endif


namespace detail {

class _platform_endpoint_buffer
{
protected:
	size_t m_size;

	// The size of this buffer is enough to store any single socket address. Additionally on Windows
	// it is enough for accepting connections on an ipv4 or ipv6 listen socket, but not on a local
	// (unix) listen socket. Accepting a local connection on Windows using this type requires
	// dynamic allocation.
	alignas(platform_endpoint_alignment) std::byte m_data[120];


	_platform_endpoint_buffer()
		: m_size(sizeof(m_data))
	{
	}

	void set_dynamic_storage(void* const dynamic_storage)
	{
		std::memcpy(m_data, &dynamic_storage, sizeof(dynamic_storage));
	}

	[[nodiscard]] void* get_dynamic_storage() const
	{
		void* dynamic_storage;
		std::memcpy(&dynamic_storage, m_data, sizeof(dynamic_storage));
		return dynamic_storage;
	}
};

} // namespace detail

template<typename Allocator>
class basic_endpoint_storage final : detail::_platform_endpoint_buffer
{
	vsm_no_unique_address Allocator m_allocator;

public:
	basic_endpoint_storage()
		requires std::is_default_constructible_v<Allocator> = default;

	template<vsm::any_cvref_of<Allocator> AllocatorArgument>
	explicit basic_endpoint_storage(AllocatorArgument&& allocator)
		: m_allocator(allocator)
	{
	}

	basic_endpoint_storage(basic_endpoint_storage const&) = delete;
	basic_endpoint_storage& operator=(basic_endpoint_storage const&) = delete;

	~basic_endpoint_storage()
	{
		if (m_size != sizeof(m_data))
		{
			m_allocator.deallocate(get_dynamic_storage(), m_size);
		}
	}


	[[nodiscard]] vsm::result<void*> resize(size_t const size)
	{
		if (size > m_size)
		{
			vsm_try(storage, allocate(size));

			if (m_size > sizeof(m_data))
			{
				m_allocator.deallocate(get_dynamic_storage(), m_size);
			}

			set_dynamic_storage(storage);
			m_size = size;

			return storage;
		}

		if (m_size > sizeof(m_data))
		{
			return get_dynamic_storage();
		}

		return m_data;
	}

private:
	[[nodiscard]] vsm::result<void*> allocate(size_t const size) noexcept
	{
		vsm_except_try
		{
			return m_allocator.allocate(size);
		}
		vsm_except_catch (std::bad_alloc const&)
		{
			return vsm::unexpected(error::not_enough_memory);
		}
	}
};

using endpoint_storage = basic_endpoint_storage<std::allocator<std::byte>>;

using any_endpoint_storage_provider = detail::any_aligned_storage_provider;

} // namespace allio
