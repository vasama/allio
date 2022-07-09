#include <allio/handle.hpp>

#include "object_transaction.hpp"

using namespace allio;

result<void> handle::set_multiplexer(multiplexer* const multiplexer, multiplexer_handle_relation_provider const* const relation_provider, type_id<handle> const handle_type, bool const is_valid)
{
	if (is_valid && (m_flags.value & flags::multiplexable) == flags::none)
	{
		return allio_ERROR(error::handle_is_not_multiplexable);
	}

	if (m_multiplexer.value != nullptr)
	{
		if (is_valid)
		{
			allio_TRYV(m_multiplexer_relation.value->deregister_handle(*m_multiplexer.value, *this));
			m_multiplexer_data.value = nullptr;
		}

		m_multiplexer.value = nullptr;
		m_multiplexer_relation.value = nullptr;
	}

	if (multiplexer != nullptr)
	{
		allio_TRY(multiplexer_relation, relation_provider->find_multiplexer_handle_relation(
			multiplexer->get_type_id(), handle_type));

		if (is_valid)
		{
			allio_TRYA(m_multiplexer_data.value, multiplexer_relation->register_handle(*multiplexer, *this));
		}

		m_multiplexer.value = multiplexer;
		m_multiplexer_relation.value = multiplexer_relation;
	}

	return {};
}

result<void> handle::set_native_handle(native_handle_type const handle)
{
	object_transaction flags_transaction(m_flags.value, handle.handle_flags);
	if (multiplexer* const multiplexer = get_multiplexer())
	{
		allio_TRYV(register_handle());
	}
	flags_transaction.commit();
	return {};
}

result<handle::native_handle_type> handle::release_native_handle()
{
	if (multiplexer* const multiplexer = get_multiplexer())
	{
		allio_TRYV(deregister_handle());
	}
#ifdef __clang__ //TODO: Remove this when clang is fixed...
	return native_handle_type
	{
		std::exchange(m_flags.value, flags::none),
	};
#else
	return
	{
		result_value,
		std::exchange(m_flags.value, flags::none),
	};
#endif
}
