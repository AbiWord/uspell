// mytypes.h
// copyright c 2003 Raphael Finkel.
// license: Gnu Public License.

// Types needed by the uspell package: UTF8 and UCS ("wide")

#ifndef MYTYPES_H
#define MYTYPES_H

#include <ctype.h>

#	if UCSLEVEL == 2
	typedef __uint16_t wide_t;
#	else
	typedef __uint32_t wide_t;
#	endif
typedef unsigned char utf8_t;

#endif // MYTYPES_H
