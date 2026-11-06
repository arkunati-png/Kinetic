#define _GNU_SOURCE
#include <stdlib.h>

char *canonicalize_file_name(const char *path)
{
	return realpath(path, 0);
}
