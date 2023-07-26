#include <allio/path_view.hpp>

#include <allio/test/reverse_iterator.hpp>

#include <filesystem>
#include <iostream>

using namespace allio;

using namespace std;
namespace fs = std::filesystem;

template<typename Path>
static int main_template(int const argc, const char* const* const argv)
{
	static constexpr bool is_std = std::is_same_v<Path, fs::path>;

	string_view const property = argv[1];
	Path const path = Path(argv[2]);

	if (property == "eq")
	{
		if (argc <= 3) return 1;
		cout << (path == Path(argv[3])) << endl; return 0;
	}

	if (property == "cmp")
	{
		if (argc <= 3) return 1;
		cout << path.compare(Path(argv[3])) << endl; return 0;
	}

	if constexpr (is_std)
	{
		if (property == "hash")
		{
			cout << hash_value(path) << endl; return 0;
		}

		if (property == "cat")
		{
			if (argc <= 3) return 1;
			cout << (path / Path(argv[3])).string() << endl; return 0;
		}
	}

#define PROPERTY(x) \
	if (property == #x) \
	{ \
		cout << path.x() << endl; \
		return 0; \
	}

	PROPERTY(is_absolute);
	PROPERTY(is_relative);
#undef PROPERTY

#define PROPERTY(x) \
	if (property == #x) \
	{ \
		cout << path.x().string() << endl; \
		return 0; \
	}

	PROPERTY(root_name);
	PROPERTY(root_path);
	PROPERTY(root_directory);
	PROPERTY(relative_path);
	PROPERTY(parent_path);
	PROPERTY(filename);
	PROPERTY(stem);
	PROPERTY(extension);

	if constexpr (is_std)
	{
		PROPERTY(lexically_normal);
	}
#undef PROPERTY

#define PROPERTY(x) \
	if (property == #x) \
	{ \
		if (argc <= 3) return 1; \
		cout << path.x(argv[3]).string() << endl; \
		return 0; \
	}

	if constexpr (is_std)
	{
		PROPERTY(lexically_relative);
		PROPERTY(lexically_proximate);
	}
#undef PROPERTY

	if (property == "iter")
	{
		int i = 0;
		for (auto const& x : path)
		{
			if (i == 8) break;
			cout << i++ << ": ";
			cout << x.string() << endl;
		}
		return 0;
	}

	if (property == "riter")
	{
		int i = 0;
		for (auto const& x : reverse_range(path))
		{
			if (i == 8) break;
			cout << i++ << ": ";
			cout << x.string() << endl;
		}
		return 0;
	}

	cout << "invalid operation" << endl;
	return 1;
}

int main(int const argc, const char* const* const argv)
{
	if (argc <= 2) return 1;

	return string_view(argv[1]) == "allio"
		? main_template<path_view>(argc - 1, argv + 1)
		: main_template<fs::path>(argc, argv);
}
