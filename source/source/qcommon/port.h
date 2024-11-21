#ifndef PORT_H
#define PORT_H

#ifndef _WIN32
	#define strcmpi strcasecmp
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
	#define __cdecl
#endif

#ifdef __GNUC__
	#ifdef __i386__
		#define EXPORT      __attribute__(( visibility( "default" ), force_align_arg_pointer ))
		#define GAME_EXPORT __attribute__(( force_align_arg_pointer ))
	#else
		#define EXPORT      __attribute__(( visibility( "default" )))
		#define GAME_EXPORT
	#endif
#elif defined( _MSC_VER )
	#define EXPORT      __declspec( dllexport )
	#define GAME_EXPORT
#endif

#endif // PORT_H
