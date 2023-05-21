#ifndef allio_detail_linux_undef
#	define allio_detail_linux_undef
#	pragma push_macro("linux")
#	ifdef linux
#		undef linux
#	endif
#else
#	undef allio_detail_linux_undef
#	pragma pop_macro("linux")
#endif

#ifndef allio_detail_unix_undef
#	define allio_detail_unix_undef
#	pragma push_macro("unix")
#	ifdef unix
#		undef unix
#	endif
#else
#	undef allio_detail_unix_undef
#	pragma pop_macro("unix")
#endif
