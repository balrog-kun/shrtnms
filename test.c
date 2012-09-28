/*
 * Written by: Andrzej Zaborowski <andrew.zaborowski@intel.com>
 *
 * Code in this file is for now licensed under the 2-clause BSD license.
 */

#include <stdio.h>

#include "shortnames.h"

int main(int argc, const char *argv[])
{
	char full_name[512];
	char short_name[512];
	char shortest_name[512];
	int ret;

	utf_init();

	while ((ret = read(0, full_name, 256)) > 0) {
		full_name[ret] = 0;
		shorten_name(full_name, short_name, shortest_name);

		printf("%s", short_name);
		printf("%s", shortest_name);
	}

	utf_done();

	return 0;
}
