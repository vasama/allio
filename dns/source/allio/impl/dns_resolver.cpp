#include <allio/dns_resolver.hpp>

#include <bit>
#include <concepts>

using namespace allio;

namespace {

template<std::unsigned_integral T>
class dns_uint
{
	uint8_t m_octets[sizeof(T)];
};

template<typename T>
static constexpr dns_uint<T> dns_convert(T const value)
{
	return std::bit_cast<dns_uint<T>>(network_byte_order(value));
}

template<typename T>
static constexpr T dns_convert(dns_uint<T> const value)
{
	return network_byte_order(std::bit_cast<T>(value));
}

using dns_u16 = dns_uint<uint16_t>;
using dns_u32 = dns_uint<uint32_t>;


namespace dns_type
{
	static constexpr uint16_t a                                 = 1;
	static constexpr uint16_t ns                                = 2;
	static constexpr uint16_t cname                             = 5;
	static constexpr uint16_t aaaa                              = 28;
}

namespace dns_class
{
	static constexpr uint16_t in                                = 1;
}

struct dns_header
{
	static constexpr uint16_t response                          = 0x8000;
	static constexpr uint16_t opcode_mask                       = 0x7800;
	static constexpr uint16_t authoritative_answer              = 0x0400;
	static constexpr uint16_t truncation                        = 0x0200;
	static constexpr uint16_t recursion_desired                 = 0x0100;
	static constexpr uint16_t recursion_available               = 0x0080;
	static constexpr uint16_t result_mask                       = 0x000F;

	dns_u16 transaction_id;
	dns_u16 bitfield;
	dns_u16 question_count;
	dns_u16 answer_count;
	dns_u16 name_server_count;
	dns_u16 additional_count;
};

struct dns_question
{
	dns_u16 question_type;
	dns_u16 question_class;
};

struct dns_resource_record
{
	dns_u16 record_type;
	dns_u16 record_class;
	dns_u32 time_to_live;
	dns_u16 data_size;
};

} // namespace


#if 0
struct query_buffer
{
	uint8_t* pos;
	uint8_t* end;

	uint8_t* reserve(size_t const size)
	{
		vsm_assert(static_cast<ptrdiff_t>(size) < end - pos);

		uint8_t* const pos = this->pos;
		this->pos = pos + size;
		return pos;
	}
};

template<typename T> //TODO: Trivially serialisable constraint.
static void write(query_buffer& buffer, T const& object)
{
	memcpy(buffer.reserve(sizeof(T)), &object, sizeof(T));
}

static void write_domain_name(query_buffer& buffer, std::string_view const domain_name)
{
	char const* const beg = domain_name.data();
	char const* end = beg + domain_name.size();

	size_t const write_size = domain_name.size() + 1;
	uint8_t* const write_beg = buffer.reserve(write_size);
	uint8_t* write_end = write_beg + write_size;

	// Root domain label size.
	*--write_end = 0;

	for (uint8_t label_size = 0; beg != end;)
	{
		char const character = *--end;

		if (character == '.')
		{
			*--write_end = label_size;
			label_size = 0;
		}
		else
		{
			*--write_end = character;
			++label_size;
		}
	}

	vsm_assert(write_beg == write_end);
}

struct query_packet_info
{
	size_t packet_size;
	size_t type_offset;
};

static query_packet_info write_query_packet(query_buffer& buffer,
	uint16_t const transaction_id, std::string_view const domain_name)
{
	write(buffer, dns_header
	{
		.transaction_id = network_byte_order(transaction_id),
		.bitfield = network_byte_order(dns_header::recursion_desired),
		.question_count = network_byte_order(1),
	});

	write_domain_name(buffer, domain_name);
	write(buffer, dns_question
	{
		.type = network_byte_order(0),
		.class_ = network_byte_order(dns_class::in),
	});
}
#endif


template<typename Engine>
static uint16_t pick_transaction_id(vsm::wb_tree<dns_query> const& tree, Engine& engine)
{
	//TODO: Handle full tree.

	auto const weight = [&](dns_query const* const node) -> uint16_t
	{
		return static_cast<uint16_t>(node != nullptr ? tree.weight(node) : 0);
	};

	uint16_t lower_bound = 0;
	uint16_t space = static_cast<size_t>(0xffff) - tree.size(); //TODO: Should this be 0x10000 - ?
	uint16_t index = std::uniform_int_distribution<uint16_t>(0, space)(engine);

	for (dns_query const* root = tree.root(); root != nullptr;)
	{
		uint16_t const root_value = root->transaction_id;
		auto const [left, right] = tree.children(root);

		uint16_t const left_range = root_value - lower_bound;
		uint16_t const left_space = left_range - weight(left);

		if (index < left_space)
		{
			space = left_space;
			root = left;
		}
		else
		{
			lower_bound = root_value;
			space -= left_space;
			index -= left_space;
			root = right;
		}
	}

	return lower_bound + index;
}

static size_t write_domain_name(std::string_view const domain_name, uint8_t* const buffer_beg)
{
	char const* const beg = domain_name.data();
	char const* end = beg + domain_name.size();

	size_t const binary_domain_name_size = domain_name.size() + 1;
	uint8_t* buffer_end = buffer_beg + binary_domain_name_size;

	// Root domain label size.
	*--buffer_end = 0;

	for (uint8_t label_size = 0; beg != end;)
	{
		vsm_assert(buffer_beg != buffer_end);
		char const character = *--end;

		if (character == '.')
		{
			*--buffer_end = label_size;
			label_size = 0;
		}
		else
		{
			*--buffer_end = character;
			++label_size;
		}
	}

	vsm_assert(buffer_beg == buffer_end);
	return binary_domain_name_size;
}

vsm::result<void> dns_query::start_query(dns_query* const query)
{
	uint16_t const transaction_id = pick_transaction_id(m_queries, m_random_engine);

	dns_header const header =
	{
		.transaction_id = dns_convert(transaction_id),
		.bitfield = dns_convert(dns_header::recursion_desired),
		.question_count = dns_convert<uint16_t>(1),
	};

	uint8_t binary_domain_name[256];
	size_t const binary_domain_name_size =
		write_domain_name(query->domain_name, binary_domain_name);

	dns_question const question_a =
	{
		.question_type = dns_convert(dns_type::a),
		.question_class = dns_convert(dns_class::in),
	};
	write_buffer const write_buffers_a[] =
	{
		as_write_buffer(&header),
		as_write_buffer(binary_domain_name, binary_domain_name_size),
		as_write_buffer(&question_a),
	};

	dns_question const question_aaaa =
	{
		.question_type = dns_convert(dns_type::a),
		.question_class = dns_convert(dns_class::in),
	};
	write_buffer const write_buffers_aaaa[] =
	{
		as_write_buffer(&header),
		as_write_buffer(binary_domain_name, binary_domain_name_size),
		as_write_buffer(&question_aaaa),
	};

	packet_write_buffer const write_buffers[] =
	{
		{ write_buffers(write_buffers_a, 3), &address },
		{ write_buffers(write_buffers_aaaa, 3), &address },
	};

	vsm_try_void(m_udp_socket.write(packet_write_buffers(write_buffers)));
	vsm_verify(m_queries.insert(query).inserted);

	return {};
}


struct response_buffer
{
	uint8_t const* beg;
	uint8_t const* pos;
	uint8_t const* end;

	uint8_t const* consume(size_t const size);
};

template<typename T>
static vsm::result<T> read_struct(response_buffer& buffer)
{
	vsm::result<T> r(vsm::result_value);
	if (static_cast<ptrdiff_t>(sizeof(T)) > buffer.end - buffer.pos)
	{
		return std::unexpected(malformed_packet);
	}
	memcpy(&*r, buffer.pos, sizeof(T));
	buffer.pos += sizeof(T);
}

static vsm::result<std::string_view> read_domain_name(response_buffer& buffer, domain_name& name);

static vsm::result<response_buffer> consume_record_data(response_buffer& buffer, size_t const size)
{
	uint8_t const* const data = buffer.consume(size);
	return response_buffer{ buffer.beg, data, data + size };
}


vsm::result<void> dns_resolver::handle_query_result(network_address const& server_address, std::span<uint8_t const> const packet)
{
	vsm_try(header, read_struct<dns_header>(buffer));
	uint16_t const bitfield = dns_convert(header.bitfield);

	// Check required bitfield values.
	{
		static constexpr required_bitfield_mask =
			dns_header::response | dns_header::opcode_mask | dns_header::recursion_desired;

		// The packet must represent a response and we always ask for recursion.
		static constexpr required_bitfield_value =
			dns_header::response | dns_header::recursion_desired;

		if ((bitfield & required_bitfield_mask) != required_bitfield_value)
		{
			return std::unexpected(unsolicited_packet);
		}
	}


	// Find the pending query by the transaction id in the response.
	dns_query* const query = m_queries.find(dns_convert(header.transaction_id));

	if (query == nullptr)
	{
		return std::unexpected(invalid_response);
	}

	if (server_address != query->m_server_address)
	{
		return std::unexpected(invalid_response);
	}


	// Check the response question validity.
	vsm_try(question_type, [&]() -> vsm::result<uint16_t>
	{
		// Query packets contain exactly one question each.
		if (dns_convert(header.question_count) != 1)
		{
			return std::unexpected(invalid_response);
		}

		domain_name question_name;
		vsm_try_void(read_domain_name(buffer, question_name));
		vsm_try(question, read_struct<dns_question>(buffer));

		if (question.question_class != dns_class::in)
		{
			return std::unexpected(invalid_response);
		}

		//TODO: Check that this record type is actually being resolved by this query.

		if (domain_name != query->m_domain_name)
		{
			return std::unexpected(invalid_response);
		}

		return question.question_type;
	}());

	std::optional<uint16_t> address_record_offset;
	std::optional<uint16_t> canonical_record_offset;
	std::optional<uint16_t> authority_record_offset;
	bool got_ns = false;
	bool got_cname = false;

	domain_name canonical_name;
	ipv4_address answer_address_a;
	ipv6_address answer_address_aaaa;

	for (size_t i = 0, c = dns_convert(header.answer_count); i < c; ++i)
	{
		domain_name record_name;
		vsm_try_void(read_domain_name(buffer, record_name));
		vsm_try(record, read_struct<dns_resource_record>(buffer));
		vsm_try(record_data, consume_record_data(buffer, record.data_size));

		if (record.record_class != dns_class::in)
		{
			return std::unexpected(invalid_response);
		}

		switch (record.record_type)
		{
		case dns_type::a:
			break;

		case dns_type::aaaa:
			break;

		case dns_type::cname:
			break;

		case dns_type::ns:
			break;
		}

		if (record_name == query->m_domain_name)
		{
			
		}
	}
}
