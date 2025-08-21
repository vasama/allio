#include <allio/blocking/standard_stream.hpp>

using namespace allio;
using namespace allio::detail;

blocking::standard_stream_handle blocking::standard_stream::cin(
	adopt_handle,
	make_standard_handle(0));

blocking::standard_stream_handle blocking::standard_stream::cout(
	adopt_handle,
	make_standard_handle(1));

blocking::standard_stream_handle blocking::standard_stream::cerr(
	adopt_handle,
	make_standard_handle(2));
