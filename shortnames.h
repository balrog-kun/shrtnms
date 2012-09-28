/*
 * Written by: Andrzej Zaborowski <andrew.zaborowski@intel.com>
 *
 * Code in this file is for now licensed under the 2-clause BSD license.
 */

void utf_init(void);
void utf_done(void);

void shorten_name(const char *name,
		char short_name[512], char shortest_name[512]);

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))
