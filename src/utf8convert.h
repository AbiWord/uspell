// utf8convert.h
// copyright c 2003 Raphael Finkel.
// license: Gnu Public License.

#include "mytypes.h"

#ifndef UTF8CONVERT_H
#define UTF8CONVERT_H

int utf8_wide(wide_t *dest, const utf8_t *source, const int outLength);
int wide_utf8(utf8_t *dest, int destLength, const wide_t *source,
	int sourceLength);
extern utf8_t *makeUTF(const wide_t *source, int sourceLength);

#endif
