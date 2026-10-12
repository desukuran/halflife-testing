// MSVC runtime names the SDK's studiomdl uses, for gcc on Linux. See build_studiomdl.sh.
#pragma once
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

#define stricmp strcasecmp
#define strcmpi strcasecmp
#define strnicmp strncasecmp
#define _strnicmp strncasecmp
#define getwd( o ) getcwd( ( o ), 256 )
#define _mkdir( p ) mkdir( ( p ), 0777 )
