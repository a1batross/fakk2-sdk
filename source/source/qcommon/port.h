#ifndef PORT_H
#define PORT_H

#ifndef _WIN32
	#define strcmpi strcasecmp
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
	#define __cdecl
#endif

#ifdef __GNUC__
	#define EXPORT __attribute__(( visibility( "default" )))
#elif defined( _MSC_VER )
	#define EXPORT __declspec( dllexport )
#else
	#define EXPORT
#endif

#endif // PORT_H
