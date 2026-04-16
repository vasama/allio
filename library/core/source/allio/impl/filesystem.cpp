#include <allio/impl/filesystem.hpp>

#include <allio/impl/hexadecimal.hpp>
#include <allio/impl/random.hpp>
#include <allio/step_deadline.hpp>

using namespace allio;
using namespace allio::detail;

namespace {

class unique_file_name
{
	using char_type = platform_path_view::value_type;

	static constexpr size_t data_bits = 128;
	static constexpr size_t data_size = data_bits / CHAR_BIT;
	static constexpr size_t name_size = hexadecimal_size(data_size);

	char_type m_name[name_size + 1];

public:
	unique_file_name() = default;

	unique_file_name(unique_file_name const&) = delete;
	unique_file_name& operator=(unique_file_name const&) = delete;

	[[nodiscard]] vsm::result<any_path_view> generate() &
	{
		std::byte data[data_size];
		vsm_try_void(secure_random_fill(data));

		vsm_verify(make_hexadecimal(data, std::span(m_name, name_size)) == name_size);
		m_name[name_size] = char_type('\0');

		//TODO: Set null terminated flag.
		return any_path_view(platform_path_view(m_name, name_size));
	}
};

} // namespace

vsm::result<unique_handle> detail::open_unique_file(
	platform_handle_type const base,
	platform_open_options const& options)
{
	//TODO: Deadline
	deadline const relative_deadline = deadline::never();
	step_deadline absolute_deadline(relative_deadline);

	unique_file_name name;

	while (true)
	{
		vsm_try_discard(absolute_deadline.step());
		vsm_try(random_file_name, name.generate());

		if (auto r = detail::open_file(base, random_file_name, options))
		{
			return r;
		}
	}
}
