#pragma once

#include <allio/deadline.hpp>

#include <vsm/preprocessor.h>

namespace allio {
namespace detail {

template<typename>
struct allio_parameters_base {};

template<typename T>
concept interface_parameters = requires { typename T::allio_interface_parameters_tag; };


#define allio_detail_parameters_type(path, name) \
	name
#define allio_detail_parameters_base(path, name) \
	, ::path::name
#define allio_detail_parameters_data(type, name, ...) \
	type name;
#define allio_detail_parameters_base_init(path, name) \
	, ::path::name(allio_detail_parameters_args)
#define allio_detail_parameters_data_init(type, name, ...) \
	, name(allio_detail_parameters_args.name)

#define allio_detail_parameters_prologue(name, definition, injection) \
	struct name \
		: ::allio::detail::allio_parameters_base<name> \
		definition(vsm_pp_empty, vsm_pp_empty, allio_detail_parameters_base, vsm_pp_empty) \
	{ \

#define allio_detail_parameters_epilogue(name, definition, injection) \
		definition(vsm_pp_empty, allio_detail_parameters_data, vsm_pp_empty, vsm_pp_empty) \
		injection(vsm_pp_empty, allio_detail_parameters_data, vsm_pp_empty, vsm_pp_empty) \
		name() = default; \
		name(::allio::detail::interface_parameters auto const& allio_detail_parameters_args) \
			: ::allio::detail::allio_parameters_base<name>{} \
			definition(vsm_pp_empty, vsm_pp_empty, allio_detail_parameters_base_init, vsm_pp_empty) \
			definition(vsm_pp_empty, allio_detail_parameters_data_init, vsm_pp_empty, vsm_pp_empty) \
			injection(vsm_pp_empty, allio_detail_parameters_data_init, vsm_pp_empty, vsm_pp_empty) {} \
	} \

#define allio_detail_parameters(name, definition) \
	allio_detail_parameters_prologue(name, definition, vsm_pp_empty) \
	allio_detail_parameters_epilogue(name, definition, vsm_pp_empty) \

#define allio_parameters(definition) \
	allio_detail_parameters(definition(allio_detail_parameters_type, vsm_pp_empty, vsm_pp_empty, vsm_pp_empty), definition)


#define allio_detail_interface_parameters_data(type, name, ...) \
	type name __VA_OPT__(= __VA_ARGS__);

#define allio_detail_interface_parameters(name, definition, injection) \
	allio_detail_parameters_prologue(name, definition, injection) \
		struct interface \
		{ \
			using allio_interface_parameters_tag = void; \
			definition(vsm_pp_empty, allio_detail_interface_parameters_data, vsm_pp_empty, allio_detail_interface_parameters_data) \
			injection(vsm_pp_empty, allio_detail_interface_parameters_data, vsm_pp_empty, allio_detail_interface_parameters_data) \
		}; \
	allio_detail_parameters_epilogue(name, definition, injection) \

#define allio_detail_interface_parameters_injection(type, data, ...) \
	data(::allio::deadline, deadline) \

#define allio_interface_parameters(definition) \
	vsm_pp_expand(allio_detail_interface_parameters( \
		definition(allio_detail_parameters_type, vsm_pp_empty, vsm_pp_empty, vsm_pp_empty), \
		definition, allio_detail_interface_parameters_injection))

} // namespace detail

#define allio_basic_parameters(type, data, ...) \
	type(allio, basic_parameters) \

allio_interface_parameters(allio_basic_parameters);

template<typename T, typename Parameters>
concept parameters = true;

} // namespace allio
