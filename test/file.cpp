#include <allio/file_handle.hpp>

#include <cstring>

int main()
{
	auto const r = []() -> allio::result<void>
	{
		// Create a default multiplexer.
		allio_TRY(multiplexer, allio::create_default_multiplexer());

		// Create null file handle and associate the multiplexer.
		allio::file_handle file;
		allio_TRYV(file.set_multiplexer(multiplexer.get()));

		// Open the test file.
		//allio_TRYV(file.open("/mnt/d/Code/allio/test/allio.txt"));
		allio_TRYV(file.open("\\??\\D:\\Code\\allio\\test\\allio.txt"));

		char buffer[] = "trash";

		allio_TRY(read_size, file.read_at(0, allio::as_read_buffer(buffer, 5)));

		if (read_size != 5)
			return printf("read size: %zu\n", read_size), allio_ERROR(make_error_code(std::errc::io_error));

		if (memcmp(buffer, "allio", 5) != 0)
			return printf("read data: %s\n", buffer), allio_ERROR(make_error_code(std::errc::io_error));

		printf("read: %s\n", buffer);

		return {};
	}();

	if (!r)
	{
		puts(r.error().message().c_str());
		return 1;
	}

	return 0;
}
