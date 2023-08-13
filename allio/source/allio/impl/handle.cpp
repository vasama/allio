#include <allio/handle.hpp>

using namespace allio;

vsm::result<void> handle::set_multiplexer(multiplexer* const multiplexer, multiplexer_handle_relation_provider const* const relation_provider)
{
	bool const not_null = (m_flags.value & flags::not_null) != handle_flags::none;

	if (m_multiplexer.value != nullptr)
	{
		if (not_null)
		{
			vsm_try_void(m_multiplexer_relation.value->deregister_handle(*m_multiplexer.value, *this));
		}

		m_multiplexer.value = nullptr;
		m_multiplexer_relation.value = nullptr;
	}

	if (multiplexer != nullptr)
	{
		vsm_try(multiplexer_relation, relation_provider
			->find_multiplexer_handle_relation(multiplexer->get_type_id(), get_type_id()));

		if (not_null)
		{
			vsm_try_void(multiplexer_relation->register_handle(*multiplexer, *this));
		}

		m_multiplexer.value = multiplexer;
		m_multiplexer_relation.value = multiplexer_relation;
	}

	return {};
}

bool handle::check_native_handle(native_handle_type const& handle)
{
	return handle.flags[flags::not_null];
}

void handle::set_native_handle(native_handle_type const& handle)
{
	m_flags.value = handle.flags;
}

handle::native_handle_type handle::release_native_handle()
{
	return
	{
		m_flags.release(),
	};
}
