#pragma once

namespace allio {

template<typename T>
class object_transaction
{
	T* m_object;
	T m_rollback;

public:
	template<typename Value>
	explicit object_transaction(T& object, Value&& value)
		: m_object(&object)
		, m_rollback(static_cast<T&&>(object))
	{
		object = static_cast<Value&&>(value);
	}

	object_transaction(object_transaction const&) = delete;
	object_transaction& operator=(object_transaction const&) = delete;

	~object_transaction()
	{
		if (m_object != nullptr)
		{
			*m_object = static_cast<T&&>(m_rollback);
		}
	}

	void commit()
	{
		m_object = nullptr;
	}
};

template<typename T, typename Value>
object_transaction(T& object, Value&& value) -> object_transaction<T>;

} // namespace allio
