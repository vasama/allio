#include <allio/sanitizer.h>

#include <cstdio>

void ::allio_sanitizer_report_error(const char* message)
{
	std::fprintf(stderr, "%s", message);
}
