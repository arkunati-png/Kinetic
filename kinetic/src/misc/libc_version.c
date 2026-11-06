#define _GNU_SOURCE
#include <gnu/libc-version.h>

#define KN_STR_(x) #x
#define KN_STR(x) KN_STR_(x)

const char *gnu_get_libc_version(void)
{
	return "kinetic-" KN_STR(__KINETIC_MAJOR__) "." KN_STR(__KINETIC_MINOR__)
		"." KN_STR(__KINETIC_PATCH__);
}

const char *gnu_get_libc_release(void)
{
	return "kinetic";
}
