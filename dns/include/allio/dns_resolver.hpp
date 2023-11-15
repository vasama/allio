#pragma once

#include <allio/datagram_socket.hpp>

#include <vsm/intrusive/wb_tree.hpp>
#include <vsm/standard.hpp>

#include <exec/sequence/iterate.hpp>
#include <exec/sequence/merge_each.hpp>
#include <exec/sequence_senders.hpp>

namespace allio {

enum class dns_error
{
	unsolicited_response,
	invalid_response,
};

template<typename T>
dns_result = vsm::result<T, dns_error>;

class domain_name
{
	static constexpr size_t max_fqdn_size = 255;

	uint8_t m_size;
	char m_data[max_fqdn_size];

public:
	explicit domain_name(std::string_view const name)
	{
		
	}

	operator std::string_view() const
	{
		return std::string_view(m_data, m_size);
	}
};

struct dns_response
{
	uint32_t time_to_live;
	bool is_referral;
	domain_name name;
	network_endpoint address;
};

enum class dns_record_type : uint8_t
{
	ipv4_address,
	ipv6_address,
	canonical_name,
	nameserver_name,
};

using dns_ttl_type = uint32_t;

class dns_record
{
	dns_record_type m_type;
	dns_ttl_type m_time_to_live;
	union
	{
		allio::ipv4_address m_ipv4_address;
		allio::ipv6_address m_ipv6_address;
		domain_name m_name;
	};

public:
	struct canonical_name_t {};
	struct nameserver_name_t {};


	explicit dns_record(dns_ttl_type const time_to_live, ipv4_address const& address)
		: m_type(dns_record_type::ipv4_address)
		, m_ttl(time_to_live)
		, m_ipv4_address(address)
	{
	}

	explicit dns_record(dns_ttl_type const time_to_live, ipv6_address const& address)
		: m_type(dns_record_type::ipv6_address)
		, m_ttl(time_to_live)
		, m_ipv6_address(address)
	{
	}

	explicit dns_record(dns_ttl_type const time_to_live, canonical_name_t, std::string_view const name)
		: m_type(dns_record_type::canonical_name)
		, m_ttl(time_to_live)
		, m_name(name)
	{
	}

	explicit dns_record(dns_ttl_type const time_to_live, nameserver_name_t, std::string_view const name)
		: m_type(dns_record_type::nameserver_name)
		, m_ttl(time_to_live)
		, m_name(name)
	{
	}


	[[nodiscard]] dns_record_type type() const
	{
		return m_type;
	}

	[[nodiscard]] bool is_ip_address() const
	{
		return
			m_type == dns_record_type::ipv4_address &&
			m_type == dns_record_type::ipv6_address;
	}

	[[nodiscard]] bool is_ipv4_address() const
	{
		return m_type == dns_record_type::ipv4_address;
	}

	[[nodiscard]] bool is_ipv6_address() const
	{
		return m_type == dns_record_type::ipv6_address;
	}

	[[nodiscard]] bool is_canonical_name() const
	{
		return m_type == dns_record_type::canonical_name;
	}

	[[nodiscard]] bool is_nameserver_name() const
	{
		return m_type == dns_record_type::nameserver_name;
	}

	[[nodiscard]] dns_ttl_type time_to_live() const
	{
		return m_time_to_live;
	}


	[[nodiscard]] allio::ip_address const& ip_address() const
	{
		vsm_assert(is_ip_address());
		return is_ipv4_address()
			? m_ipv4_address
			: m_ipv6_address;
	}

	[[nodiscard]] allio::ipv4_address const& ipv4_address() const
	{
		vsm_assert(is_ipv4_address());
		return m_ipv4_address;
	}

	[[nodiscard]] allio::ipv6_address const& ipv6_address() const
	{
		vsm_assert(is_ipv6_address());
		return m_ipv6_address;
	}

	[[nodiscard]] domain_name const& canonical_name() const
	{
		vsm_assert(is_canonical_name());
		return m_name;
	}

	[[nodiscard]] domain_name const& nameserver_name() const
	{
		vsm_assert(is_nameserver_name());
		return m_name;
	}
};

namespace detail {

class dns_response_parser
{
	uint8_t const* m_beg;
	uint8_t const* m_pos;
	uint8_t const* m_end;

	uint16_t m_transaction_id;
	uint16_t m_answer_count;

public:
	[[nodiscard]] static dns_result<dns_response_parser> parse(std::span<uint8_t const> response_data);

	[[nodiscard]] dns_result<dns_response> parse_record();

private:
	dns_response_parser() = default;
};

struct dns_resolve_parameters
{
	std::string_view domain_name;
	network_endpoint server_endpoint;
};

struct dns_query : vsm::wb_tree_link
{
	dns_resolve_parameters m_args;

	uint16_t m_transaction_id;
	uint16_t m_record_type_mask;

	dns_query_buffer m_buffer;

	void(*m_process_response)(dns_query& query, dns_response_parser& parser);
};

template<typename MultiplexerHandle>
class basic_dns_client
{
	static constexpr network_port_t default_server_port = 53;

	using random_engine_type = vsm::pcg32;
	using udp_socket_type = async::basic_datagram_socket_handle<MultiplexerHandle>;

	random_engine_type m_random_engine;
	vsm::intrusive::wb_tree<dns_query> m_queries;

	udp_socket_type m_udp_socket;

	class response_sequence_sender
	{
		class response_sender
		{
			template<typename R>
			class operation
			{
				basic_dns_client* m_client;
				vsm_no_unique_address R m_receiver;
			
			public:
				explicit operation(response_sender const& sender, auto&& receiver)
					: m_client(sender.m_client)
					, m_receiver(receiver)
				{
				}
				
				friend void tag_invoke(ex::start_t, operation& self) noexcept
				{
					if (auto const r = parse_record())
					{
						ex::set_value(vsm_move(m_receiver), *r);
					}
					else
					{
						ex::set_error(vsm_move(m_receiver), );
					}
				}
			};

			basic_dns_client* m_client;

		public:
			explicit response_sender(basic_dns_client& client)
				: m_client(&client)
			{
			}
			
			template<typename R>
			friend operation<std::decay_t<R>> tag_invoke(ex::connect_t, response_sender const& self, R&& receiver)
			{
				return operation<std::decay_t<R>>(self, vsm_forward(receiver));
			}
		};

		template<typename R>
		class operation : dns_query
		{
			class next_receiver
			{
				operation* m_operation

			public:
				friend void tag_invoke(ex::set_value_t, next_receiver&& self)
				{
					self.m_operation->
				}

				friend void tag_invoke(ex::set_stopped_t, next_receiver&& self)
				{
					
				}
			};

			using next_sender_type = exec::next_sender_of_t<R, response_sender>;
			using next_operation_type = ex::connect_result_t<next_sender_type, next_receiver>;

			basic_dns_client* m_client;
			vsm_no_unique_address R m_receiver;

		public:
			explicit operation(auto const& sender, auto&& receiver)
				: dns_query(sender.m_args)
				, m_client(sender.m_client)
				, m_receiver(vsm_forward(receiver))
			{
			}

			friend void tag_invoke(ex::start_t, operation& self) noexcept
			{
				execute_inline(exec::set_next(self.m_receiver, response_sender(*self.m_client)), [&]()
				{
					
				});
			}
		};

		basic_dns_client* m_client;
		dns_resolve_parameters m_args;

	public:
		explicit response_sequence_sender(basic_dns_client& client, std::convertible_to<dns_resolve_parameters> auto&& args)
			: m_client(&client)
			, m_args(vsm_forward(args))
		{
		}

		template<typename R>
		friend operation<std::decay_t<R>> tag_invoke(ex::connect_t, response_sequence_sender const& self, R&& receiver)
		{
			return operation<std::decay_t<R>>(self, vsm_forward(receiver));
		}
	};

public:
	[[nodiscard]] ex::sequence_sender auto fetch_records(ip_address const server_address, std::string_view const domain_name) &
	{
		return fetch_records(network_endpoint(server_address, default_server_port), domain_name);
	}

	[[nodiscard]] ex::sequence_sender auto fetch_records(network_endpoint const& server_endpoint, std::string_view const domain_name) &
	{
		return
			response_sequence_sender(*this)
			| exec::then_each([](std::input_range auto&& records)
			{
				return exec::iterate(records);
			})
			| exec::merge_each();
	}

private:
	uint16_t pick_transaction_id() const;

	vsm::result<void> start_query(dns_query& query)
	{
		auto const query_data = format_query(query);

		m_udp_socket.send_to(query.args.server_endpoint, query_data);
	}

	vsm::result<void> handle_datagram(std::span<uint8_t const> data, network_endpoint const& source_endpoint);
};

} // namespace detail

} // namespace allio
