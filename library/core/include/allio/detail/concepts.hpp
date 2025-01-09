#pragma once

#include <vsm/concepts.hpp>

namespace allio::detail {

template<template<typename...> typename Template, typename... Args>
void _any_cvref_of_template(Template<Args...> const&);

template<typename T, template<typename...> typename Template>
concept any_cvref_of_template = requires (T const& t) { _any_cvref_of_template<Template>(t); };

} // namespace allio::detail
