#include <allio/detail/io_result.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
using namespace allio::detail;

namespace {

template<int>
class payload
{
public:
	payload()
	{
	}

	payload(payload&&)
	{
	}

	payload(payload const&)
	{
	}

	payload& operator=(payload&&)
	{
		return *this;
	}

	payload& operator=(payload const&)
	{
		return *this;
	}

	~payload()
	{
	}
};

template<typename P, typename T, typename E>
using result = io_result<typename P::template value_type<T>, typename P::template error_type<E>>;

struct TestType
{
	template<typename T>
	using value_type = T;

	template<typename E>
	using error_type = E;
};

} // namespace

//static_assert(std::is_trivially_copyable_v<io_result<int, int>>);
//static_assert(std::is_trivially_copyable_v<io_result<void, int>>);

TEST_CASE("io_result<int> can be default constructed", "[io_result]")
{
	using R = result<TestType, int, int>;

	R r;
	REQUIRE(r.has_value());
	REQUIRE(r.value() == 0);
	REQUIRE(!r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result<void> can be default constructed", "[io_result]")
{
	using R = result<TestType, void, int>;

	R r;
	REQUIRE(r.has_value());
	REQUIRE(!r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result<int> can be constructed using std::in_place", "[io_result]")
{
	using R = result<TestType, int, int>;

	R r(std::in_place);
	REQUIRE(r.has_value());
	REQUIRE(r.value() == 0);
	REQUIRE(!r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result<int> can be constructed using std::in_place and int", "[io_result]")
{
	using R = result<TestType, int, int>;

	R r(std::in_place, 42);
	REQUIRE(r.has_value());
	REQUIRE(r.value() == 42);
	REQUIRE(!r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result<void> can be constructed using std::in_place", "[io_result]")
{
	using R = result<TestType, void, int>;

	R r(std::in_place);
	REQUIRE(r.has_value());
	REQUIRE(!r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result<int> can be implicitly constructed from int", "[io_result]")
{
	using R = result<TestType, int, int>;

	R r = 42;
	REQUIRE(r.has_value());
	REQUIRE(r.value() == 42);
	REQUIRE(!r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result can be constructed using std::unexpect", "[io_result]")
{
	using R = result<TestType, void, int>;

	R r(std::unexpect);
	REQUIRE(!r.has_value());
	REQUIRE(r.error() == 0);
	REQUIRE(!r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result can be constructed using std::unexpect and error", "[io_result]")
{
	using R = result<TestType, void, int>;

	R r(std::unexpect, 42);
	REQUIRE(!r.has_value());
	REQUIRE(r.error() == 42);
	REQUIRE(!r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result can be implicitly constructed from std::unexpected", "[io_result]")
{
	using R = result<TestType, void, int>;

	R r = vsm::unexpected(42);
	REQUIRE(!r.has_value());
	REQUIRE(r.error() == 42);
	REQUIRE(!r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result can be implicitly constructed from io_pending", "[io_result]")
{
	using R = result<TestType, void, int>;

	R r = io_pending(42);
	REQUIRE(!r.has_value());
	REQUIRE(r.error() == 42);
	REQUIRE(r.is_pending());
	REQUIRE(!r.is_cancelled());
}

TEST_CASE("io_result can be implicitly constructed from io_cancelled", "[io_result]")
{
	using R = result<TestType, void, int>;

	R r = io_cancelled(42);
	REQUIRE(!r.has_value());
	REQUIRE(r.error() == 42);
	REQUIRE(!r.is_pending());
	REQUIRE(r.is_cancelled());
}

TEST_CASE("io_result can be implicitly constructed from result", "[io_result]")
{
	using R = result<TestType, void, int>;

	R r = vsm::result<void, int>();
	REQUIRE(r.has_value());
}

TEST_CASE("io_result<int> with can be move constructed")
{
	using R = result<TestType, int, int>;

	R r1 = GENERATE(R(42), R(vsm::result_error, 42));
	R r2 = vsm_move(r1);
	REQUIRE(r2.has_value() == r1.has_value());
	if (r2.has_value())
	{
		REQUIRE(r2.value() == 42);
	}
	else
	{
		REQUIRE(r2.error() == 42);
	}
}

TEST_CASE("io_result<int> with can be copy constructed")
{
	using R = result<TestType, int, int>;

	R r1 = GENERATE(R(42), R(vsm::result_error, 42));
	R r2 = r1;
	REQUIRE(r2.has_value() == r1.has_value());
	if (r2.has_value())
	{
		REQUIRE(r2.value() == 42);
	}
	else
	{
		REQUIRE(r2.error() == 42);
	}
}

TEST_CASE("io_result<int> with can be move assigned")
{
	using R = result<TestType, int, int>;

	R r1 = GENERATE(R(42), R(vsm::result_error, 42));
	R r2;
	r2 = vsm_move(r1);
	REQUIRE(r2.has_value() == r1.has_value());
	if (r2.has_value())
	{
		REQUIRE(r2.value() == 42);
	}
	else
	{
		REQUIRE(r2.error() == 42);
	}
}

TEST_CASE("io_result<int> with can be copy assigned")
{
	using R = result<TestType, int, int>;

	R r1 = GENERATE(R(42), R(vsm::result_error, 42));
	R r2;
	r2 = r1;
	REQUIRE(r2.has_value() == r1.has_value());
	if (r2.has_value())
	{
		REQUIRE(r2.value() == 42);
	}
	else
	{
		REQUIRE(r2.error() == 42);
	}
}

TEST_CASE("io_result<void> with can be move assigned")
{
	using R = result<TestType, void, int>;

	R r1 = GENERATE(R(), R(vsm::result_error, 42));
	R r2;
	r2 = vsm_move(r1);
	REQUIRE(r2.has_value() == r1.has_value());
	if (!r2.has_value())
	{
		REQUIRE(r2.error() == 42);
	}
}

TEST_CASE("io_result<void> with can be copy assigned")
{
	using R = result<TestType, void, int>;

	R r1 = GENERATE(R(), R(vsm::result_error, 42));
	R r2;
	r2 = r1;
	REQUIRE(r2.has_value() == r1.has_value());
	if (!r2.has_value())
	{
		REQUIRE(r2.error() == 42);
	}
}
