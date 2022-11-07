#pragma once

#include <allio/deadline.hpp>
#include <allio/detail/preprocessor.h>

namespace allio {
namespace detail {

template<typename>
struct allio_parameters_base {};

template<typename T>
concept interface_parameters = requires { typename T::allio_interface_parameters_tag; };


#define allio_detail_PARAMETERS_TYPE(path, name) \
	name
#define allio_detail_PARAMETERS_BASE(path, name) \
	, ::path::name
#define allio_detail_PARAMETERS_DATA(type, name, ...) \
	type name;
#define allio_detail_PARAMETERS_BASE_INIT(path, name) \
	, ::path::name(allio_PARAMETERS_args)
#define allio_detail_PARAMETERS_DATA_INIT(type, name, ...) \
	, name(allio_PARAMETERS_args.name)

#define allio_detail_PARAMETERS_PROLOGUE(name, definition, injection) \
	struct name \
		: ::allio::detail::allio_parameters_base<name> \
		definition(allio_detail_EMPTY, allio_detail_EMPTY, allio_detail_PARAMETERS_BASE, allio_detail_EMPTY) \
	{ \

#define allio_detail_PARAMETERS_EPILOGUE(name, definition, injection) \
		definition(allio_detail_EMPTY, allio_detail_PARAMETERS_DATA, allio_detail_EMPTY, allio_detail_EMPTY) \
		injection(allio_detail_EMPTY, allio_detail_PARAMETERS_DATA, allio_detail_EMPTY, allio_detail_EMPTY) \
		name() = default; \
		name(::allio::detail::interface_parameters auto const& allio_PARAMETERS_args) \
			: ::allio::detail::allio_parameters_base<name>{} \
			definition(allio_detail_EMPTY, allio_detail_EMPTY, allio_detail_PARAMETERS_BASE_INIT, allio_detail_EMPTY) \
			definition(allio_detail_EMPTY, allio_detail_PARAMETERS_DATA_INIT, allio_detail_EMPTY, allio_detail_EMPTY) \
			injection(allio_detail_EMPTY, allio_detail_PARAMETERS_DATA_INIT, allio_detail_EMPTY, allio_detail_EMPTY) {} \
	} \

#define allio_detail_PARAMETERS(name, definition) \
	allio_detail_PARAMETERS_PROLOGUE(name, definition, allio_detail_EMPTY) \
	allio_detail_PARAMETERS_EPILOGUE(name, definition, allio_detail_EMPTY) \

#define allio_PARAMETERS(definition) \
	allio_detail_PARAMETERS(definition(allio_detail_PARAMETERS_TYPE, allio_detail_EMPTY, allio_detail_EMPTY, allio_detail_EMPTY), definition)


#define allio_detail_INTERFACE_PARAMETERS_DATA(type, name, ...) \
	type name __VA_OPT__(= __VA_ARGS__);

#define allio_detail_INTERFACE_PARAMETERS(name, definition, injection) \
	allio_detail_PARAMETERS_PROLOGUE(name, definition, injection) \
		struct interface \
		{ \
			using allio_interface_parameters_tag = void; \
			definition(allio_detail_EMPTY, allio_detail_INTERFACE_PARAMETERS_DATA, allio_detail_EMPTY, allio_detail_INTERFACE_PARAMETERS_DATA) \
			injection(allio_detail_EMPTY, allio_detail_INTERFACE_PARAMETERS_DATA, allio_detail_EMPTY, allio_detail_INTERFACE_PARAMETERS_DATA) \
		}; \
	allio_detail_PARAMETERS_EPILOGUE(name, definition, injection) \

#define allio_detail_INTERFACE_PARAMETERS_INJECTION(type, data, ...) \
	data(::allio::deadline, deadline) \

#define allio_INTERFACE_PARAMETERS(definition) \
	allio_detail_EXPAND(allio_detail_INTERFACE_PARAMETERS( \
		definition(allio_detail_PARAMETERS_TYPE, allio_detail_EMPTY, allio_detail_EMPTY, allio_detail_EMPTY), \
		definition, allio_detail_INTERFACE_PARAMETERS_INJECTION))

} // namespace detail

#define allio_BASIC_PARAMETERS(type, data, ...) \
	type(allio, basic_parameters) \

allio_INTERFACE_PARAMETERS(allio_BASIC_PARAMETERS);

template<typename T, typename Parameters>
concept parameters = true;

} // namespace allio
