#include <allio/impl/secrets.hpp>

#include <Windows.h>

using namespace allio;

void allio::memzero_explicit(void* const data, size_t const size)
{
	SecureZeroMemory(data, size);
}
