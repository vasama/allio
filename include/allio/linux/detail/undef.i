#ifdef __linux__
#	ifndef allio_detail_LINUX_UNDEF
#		define allio_detail_LINUX_UNDEF
#		pragma push_macro("linux")
#		ifdef linux
#			undef linux
#		endif
#	else
#		undef allio_detail_LINUX_UNDEF
#		pragma pop_macro("linux")
#	endif
	
#	ifndef allio_detail_UNIX_UNDEF
#		define allio_detail_UNIX_UNDEF
#		pragma push_macro("unix")
#		ifdef unix
#			undef unix
#		endif
#	else
#		undef allio_detail_UNIX_UNDEF
#		pragma pop_macro("unix")
#	endif
#endif
