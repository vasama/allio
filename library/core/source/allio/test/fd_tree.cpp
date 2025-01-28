#include <allio/impl/fd_tree.hpp>

#include <catch2/catch_all.hpp>

#include <random>
#include <set>

using namespace allio;

namespace {

TEST_CASE("fd_tree")
{
	static constexpr int insertion_count = 1'000'000;

	fd_tree tree;
	for (int i = 0; i < insertion_count; ++i)
	{
		int const index = tree.allocate().value();
		REQUIRE(index == i);
	}

	auto& rng = Catch::sharedRng();
	std::uniform_int_distribution<int> distribution(0, insertion_count - 1);

	std::set<int> free_indices;
	auto remove_at = [&](int const index)
	{
		if (free_indices.insert(index).second)
		{
			tree.deallocate(index);
		}
	};

	while (free_indices.size() < insertion_count / 2)
	{
		remove_at(distribution(rng));
	}

	remove_at(0);
	remove_at(insertion_count - 1);

	for (int const free_index : free_indices)
	{
		int const index = tree.allocate().value();
		REQUIRE(index == free_index);
	}
}

} // namespace
