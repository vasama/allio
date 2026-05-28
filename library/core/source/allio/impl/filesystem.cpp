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

fs_standard_info::fs_standard_info(fs_info_view const view) noexcept
{
	m_mask = view.mask();

	if (vsm::any_flags(view.mask(), fs_info_mask::type))
	{
		m_type = view.type();
	}

	if (vsm::any_flags(view.mask(), fs_info_mask::dev_id))
	{
		m_dev_id = view.dev_id();
	}

	if (vsm::any_flags(view.mask(), fs_info_mask::ino_id))
	{
		m_ino_id = view.ino_id();
	}

	if (vsm::any_flags(view.mask(), fs_info_mask::link_count))
	{
		m_link_count = view.link_count();
	}

	if (vsm::any_flags(view.mask(), fs_info_mask::max_extent))
	{
		m_max_extent = view.max_extent();
	}

	if (vsm::any_flags(view.mask(), fs_info_mask::data_access_time))
	{
		m_data_access_time = view.data_access_time();
	}

	if (vsm::any_flags(view.mask(), fs_info_mask::data_change_time))
	{
		m_data_change_time = view.data_change_time();
	}

	if (vsm::any_flags(view.mask(), fs_info_mask::attr_change_time))
	{
		m_attr_change_time = view.attr_change_time();
	}
}
