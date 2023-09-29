#pragma once

namespace allio::win32 {

struct synchronous_t
{
	bool alert;
};

inline synchronous_t synchronous(bool const alert = false)
{
	return { alert };
}

} // namespace allio::win32
