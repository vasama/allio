#pragma once

namespace allio::detail {

template<typename Signature>
class function_view;

template<typename Result, typename... Params>
class function_view<Result(Params...)>
{
public:
	template<typename... Args>
	Result operator()(Args&&... args) const;
};

} // namespace allio::detail
