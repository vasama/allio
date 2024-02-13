#include <allio/impl/unique_name.hpp>

#include <cstdint>
#include <cstdlib>

using namespace allio;

template<typename Char>
void allio::generate_unique_name(std::span<Char> const buffer)
{
	for (Char& character : buffer)
	{
		//TODO: Use a real RNG.
		uint8_t const digit = static_cast<uint8_t>(rand());
		character = (digit < 10 ? Char('0') : Char('A') - 10) + digit;
	}
}

template void allio::generate_unique_name<char>(std::span<char> buffer);
template void allio::generate_unique_name<wchar_t>(std::span<wchar_t> buffer);
