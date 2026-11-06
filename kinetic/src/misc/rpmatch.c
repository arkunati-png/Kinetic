#define _GNU_SOURCE
#include <stdlib.h>
#include <langinfo.h>
#include <regex.h>

int rpmatch(const char *resp)
{
	regex_t re;
	int r = -1;

	if (!resp) return -1;
	if (!regcomp(&re, nl_langinfo(YESEXPR), REG_EXTENDED|REG_NOSUB)) {
		if (!regexec(&re, resp, 0, 0, 0)) r = 1;
		regfree(&re);
	}
	if (r < 0 && !regcomp(&re, nl_langinfo(NOEXPR), REG_EXTENDED|REG_NOSUB)) {
		if (!regexec(&re, resp, 0, 0, 0)) r = 0;
		regfree(&re);
	}
	return r;
}
