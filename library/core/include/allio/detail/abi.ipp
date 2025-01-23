#include <system_error>

namespace allio::detail {

class abi_error_category : public std::error_category
{
public:
	char const* name() const noexcept override;
	std::string message(int code) const override;

	std::error_condition default_error_condition(int code) const noexcept override;

	static abi_error_category const& get()
	{
		return instance;
	}

private:
	static abi_error_category const instance;
};

} // namespace allio::detail

template<>
struct std::is_error_code_enum<allio_abi_result>
{
	static constexpr bool value = true;
};

inline std::error_code make_error_code(allio_abi_result const result)
{
	return std::error_code(static_cast<int>(result), allio::detail::abi_error_category::get());
}
