#include <iostream>
#include <stacktrace>

int main()
{
	std::cerr << std::stacktrace::current() << std::endl;
}
