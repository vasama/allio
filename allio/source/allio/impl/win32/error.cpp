#include <allio/error.hpp>

#include <vsm/platform.h>
#include <vsm/preprocessor.h>

using namespace allio;

extern "C"
void allio_unrecoverable_error_default(std::error_code const error, std::source_location const location)
{
	detail::unrecoverable_error_default(error, location);
}

void detail::unrecoverable_error(std::error_code const error, std::source_location const location)
{
	allio_unrecoverable_error(error, location);
}


#if vsm_arch_x86_32
#	define allio_detail_mangle(name) vsm_pp_cat(_, name)
#else
#	define allio_detail_mangle(name) name
#endif

__pragma(comment(linker, vsm_pp_str(/ALTERNATENAME:allio_detail_mangle(allio_unrecoverable_error)=allio_detail_mangle(allio_unrecoverable_error_default))))
