#include <allio/background_multiplexer.hpp>

using namespace allio;

namespace {

template<typename Extension>
class async_operation_storage_extension : public Extension
{
	std::byte m_storage alignas(Extension)[];

public:
	static async_operation_storage_extension& cast(async_operation& operation)
	{
		return *reinterpret_cast<async_operation_storage_extension*>(
			reinterpret_cast<std::byte*>(&operation) - offsetof(async_operation_storage_extension, m_storage));
	}

	static async_operation_storage_extension const& cast(async_operation const& operation)
	{
		return *reinterpret_cast<async_operation_storage_extension const*>(
			reinterpret_cast<std::byte const*>(&operation) - offsetof(async_operation_storage_extension, m_storage));
	}

	operator async_operation&()
	{
		return reinterpret_cast<async_operation&>(m_storage);
	}

	operator async_operation const&() const
	{
		return reinterpret_cast<async_operation const&>(m_storage);
	}
};

struct extension
{
	bool synchronous;
};

struct background_operation_storage
{

};

using async_operation_storage = async_operation_storage_extension<extension>;

} // namespace

storage_requirements background_multiplexer::get_operation_storage_requirements(async_operation_descriptor const& descriptor) const
{
	return m_multiplexer->get_operation_storage_requirements(descriptor);
}

vsm::result<async_operation_ptr> background_multiplexer::construct(async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments, async_operation_listener* const listener)
{
	vsm::result<async_operation_ptr> r = m_multiplexer->construct(descriptor, storage, arguments, listener);

	if (!r && r.error() == error::unsupported_asynchronous_operation)
	{

	}

	return r;
}

vsm::result<void> background_multiplexer::start(async_operation_descriptor const& descriptor, async_operation& operation)
{
	async_operation_storage& s = async_operation_storage::cast(operation);

	if (s.synchronous)
	{
		m_executor->submit();
	}
	else
	{
		return m_multiplexer->start(descriptor, operation);
	}
}

vsm::result<async_operation_ptr> background_multiplexer::construct_and_start(async_operation_descriptor const& descriptor, storage_ptr const storage, async_operation_parameters const& arguments, async_operation_listener* const listener)
{
	vsm::result<async_operation_ptr> r = m_multiplexer->construct_and_start(descriptor, storage, arguments, listener);

	if (!r && r.error() == error::unsupported_asynchronous_operation)
	{
		
	}

	return r;
}

vsm::result<void> background_multiplexer::cancel(async_operation_descriptor const& descriptor, async_operation& operation)
{

}

vsm::result<void> background_multiplexer::block(async_operation_descriptor const& descriptor, async_operation_parameters const& arguments)
{
	return m_multiplexer->block(descriptor, arguments);
}

