#include <allio/nothrow/standard_stream.hpp>

using namespace allio;
using namespace allio::detail;

nothrow::standard_stream_handle nothrow::standard_stream::cin(
	adopt_handle,
	make_standard_handle(0));

nothrow::standard_stream_handle nothrow::standard_stream::cout(
	adopt_handle,
	make_standard_handle(1));

nothrow::standard_stream_handle nothrow::standard_stream::cerr(
	adopt_handle,
	make_standard_handle(2));
