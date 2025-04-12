#pragma once

#include <allio/any_string.hpp>
#include <allio/any_string_buffer.hpp>
#include <allio/detail/handle.hpp>

namespace allio::detail {

class serialization_context
{
protected:
	uint32_t m_version = 0;

public:
	[[nodiscard]] uint32_t version() const
	{
		return m_version;
	}

	[[nodiscard]] vsm::result<void> visit_header(std::string_view identifier, uint32_t version)
	{
		return _visit_header(identifier, version);
	}

	template<std::integral T>
		requires (sizeof(T) <= sizeof(uint64_t))
	[[nodiscard]] vsm::result<void> visit(T& value)
	{
		return _visit_integer(&value, sizeof(value), std::is_signed_v<T>);
	}

	template<vsm::enumeration Enum>
		requires (sizeof(Enum) <= sizeof(uint64_t))
	[[nodiscard]] vsm::result<void> visit(Enum& value)
	{
		return _visit_integer(
			&value,
			sizeof(value),
			std::is_signed_v<std::underlying_type_t<Enum>>);
	}

	template<typename T>
		requires (sizeof(T) <= sizeof(uint64_t)) && std::is_trivially_copyable_v<T>
	[[nodiscard]] vsm::result<void> visit_integer(T& integer)
	{
		return _visit_integer(&integer, sizeof(integer), /* is_signed: */ false);
	}

protected:
	serialization_context() = default;
	serialization_context(serialization_context const&) = delete;
	serialization_context& operator=(serialization_context const&) = delete;
	~serialization_context() = default;

	virtual vsm::result<void> _visit_header(std::string_view identifier, uint32_t version) = 0;
	virtual vsm::result<void> _visit_integer(void* ptr, size_t size, bool is_signed) = 0;
};

#if 0
class serialization_context
{
public:
	template<typename T>
		requires std::is_trivially_copyable_v<T>
	[[nodiscard]] vsm::result<void> visit(T const& value)
	{
		return _visit(std::addressof(value), sizeof(T));
	}

protected:
	serialization_context() = default;
	serialization_context(serialization_context const&) = default;
	serialization_context& operator=(serialization_context const&) = default;
	~serialization_context() = default;

private:
	vsm::result<void> _visit(void const* data, size_t size);
};

using serialize_function_type = vsm::result<void>(
	serialization_context& context,
	native_handle<object_t>& h);

vsm::result<size_t> _encode_handle(
	serialize_function_type* serialize,
	native_handle<object_t> const& h,
	any_string_buffer buffer);

vsm::result<void> _decode_handle(
	serialize_function_type* serialize,
	native_handle<object_t>& h,
	any_string_view string);

template<object Object>
[[nodiscard]] vsm::result<size_t> encode_handle(
	native_handle<Object> const& h,
	any_string_buffer const buffer)
{
	static constexpr auto serialize = [](
		serialization_context& context,
		native_handle<object_t>& h) -> vsm::result<void>
	{
		vsm_try_void(context.visit(Object::serialization_id));
		return Object::serialize(h);
	};

	return detail::_encode_handle(serialize, h, buffer);
}

template<object Object>
[[nodiscard]] vsm::result<void> decode_handle(
	native_handle<Object>& h,
	any_string_view const string)
{
	static constexpr auto serialize = [](
		serialization_context& context,
		native_handle<object_t>& h) -> vsm::result<void>
	{
		return Object::serialize(h);
	};

	return detail::_decode_handle(serialize, h, string);
}
#endif

using serialize_callback_t = vsm::result<void>(
	native_handle<object_t>& h,
	serialization_context& serializer);

vsm::result<size_t> _encode_handle(
	native_handle<object_t> const& h,
	any_string_buffer buffer,
	serialize_callback_t* serialize);

vsm::result<void> _decode_handle(
	native_handle<object_t>& h,
	any_string_view string,
	serialize_callback_t* serialize);


template<typename Object>
concept serializable_object =
	object<Object> &&
	std::same_as<typename Object::is_serializable, Object>;

template<serializable_object Object>
[[nodiscard]] vsm::result<size_t> encode_handle(
	native_handle<Object> const& h,
	any_string_buffer const buffer)
{
	return detail::_encode_handle(
		h,
		buffer,
		[](native_handle<object_t>& h, serialization_context& serializer)
		{
			return Object::serialize(static_cast<native_handle<Object>&>(h), serializer);
		});
}

template<serializable_object Object>
[[nodiscard]] vsm::result<void> decode_handle(
	native_handle<Object>& h,
	any_string_view const string)
{
	return detail::_decode_handle(
		h,
		string,
		[](native_handle<object_t>& h, serialization_context& serializer)
		{
			return Object::serialize(static_cast<native_handle<Object>&>(h), serializer);
		});
}


template<handle Handle>
	requires serializable_object<typename Handle::object_type>
[[nodiscard]] vsm::result<size_t> _encode_handle(Handle const& h, any_string_buffer const buffer)
{
	return detail::encode_handle(h.native(), buffer);
}

template<typename String, handle Handle>
	requires serializable_object<typename Handle::object_type>
[[nodiscard]] vsm::result<String> _encode_handle(Handle const& h)
{
	vsm::result<String> r(vsm::result_value);
	if (auto const r2 = detail::_encode_handle(h, *r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	return r;
}

template<detached_handle Handle>
	requires serializable_object<typename Handle::object_type>
[[nodiscard]] vsm::result<Handle> _decode_handle(any_string_view const string)
{
	using object_type = typename Handle::object_type;

	native_handle<object_type> h;
	vsm_try_void(detail::decode_handle(h, string));
	verify_handle(h);

	return rebind_handle<Handle>(basic_detached_handle<object_type>(adopt_handle, h));
}

} // namespace allio::detail
