// utf8convert.cpp
// copyright c 2003 Raphael Finkel.
// Some code taken with modification from Bram Moolenaar's Vim.
// license: Gnu Public License.
//
// Conversion routines between UTF8 and UCS (typically UCS4) representations.
//
// utf8_wide: from UTF8 to UCS
// wide_utf8: from UCS to UTF8
// makeUTF: from UCS to UTF8, places result in volatile temporary location

#include <stdlib.h>

#include "utf8convert.h"

#define LINELEN 10000

// Converters between utf8 and UCS

// from Vim code, by Bram Moolenaar

/* Lookup table to quickly get the length in bytes of a UTF-8 character from
 * the first byte of a UTF-8 string.  Bytes which are invalid when used as the
 * first byte have a one, because these will be used separately. */
static char utf8len_tab[256] =
{
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1,
};

/*
 * Convert a UTF-8 byte sequence to wide characters.  If the sequence is
 * invalid or truncated by a NUL the first byte is returned.  Return the number
 * of wide characters constructed.  Don't go beyong outLength.
 */
int utf8_wide(wide_t *dest, const utf8_t *source, int outLength){
    int		len;
	const unsigned char *p = source;
	wide_t *oldDest = dest;
	while (*p) { // one utf-8 character
		if (p[0] < 0x80) {	/* be quick for ASCII */
			if ((--outLength) == 0) break; // no more room
			*dest++ = *p++;
			continue;
		}
		len = utf8len_tab[p[0]];
		if ((outLength -= len) <= 0) break; // no more room
		if ((p[1] & 0xc0) == 0x80) {
			if (len == 2) {
				*dest++ = ((p[0] & 0x1f) << 6) + (p[1] & 0x3f);
				p += 2;
				continue;
			} else if ((p[2] & 0xc0) == 0x80) {
				if (len == 3) {
					*dest++ = ((p[0] & 0x0f) << 12) + ((p[1] & 0x3f) << 6)
						+ (p[2] & 0x3f);
					p += 3;
					continue;
				} else if ((p[3] & 0xc0) == 0x80) {
					if (len == 4) {
						*dest++ = ((p[0] & 0x07) << 18) + ((p[1] & 0x3f) << 12)
							+ ((p[2] & 0x3f) << 6) + (p[3] & 0x3f);
						p += 4;
						continue;
					} else if ((p[4] & 0xc0) == 0x80) {
						if (len == 5) {
							*dest++ = ((p[0] & 0x03)<<24) + ((p[1]& 0x3f) << 18)
								+ ((p[2] & 0x3f) << 12) + ((p[3] & 0x3f) << 6)
								+ (p[4] & 0x3f);
							p += 5;
							continue;
						} else if ((p[5] & 0xc0) == 0x80 && len == 6) {
							*dest++ = ((p[0] & 0x01) << 30)
								+ ((p[1] & 0x3f) << 24)
								+ ((p[2] & 0x3f) << 18) + ((p[3] & 0x3f) << 12)
								+ ((p[4] & 0x3f) << 6) + (p[5] & 0x3f);
							p += 6;
							continue;
						}
					} // look at p[4]
				} // look at p[3]
			} // look at p[2]
		} // look at p[1]
		/* Invalid value, just record the first byte */
		*dest++ = *p++;
	} // while *p
	return(dest - oldDest);
} // utf8_wide

/*
 * Convert a wide character string to a null-terminated UTF-8 string.  Returns
 * the number of bytes in the UTF-8 string, including the null, but not to
 * exceed outLength.  Any null wide characters are silently ignored; this rule
 * lets us convert wide strings that have omissions.
 */
int wide_utf8(utf8_t *dest, int destLength, const wide_t *source,
		int sourceLength){
	utf8_t *oldDest = dest;
	for (; sourceLength; source++, sourceLength--) {
		if (*source == 0x00) { /* 0 bits */
			// no effect
		} else if (*source < 0x80)	{	/* 7 bits */ 
			*dest++ = *source;
			destLength -= 1;
		} else if (*source < 0x800)	{	/* 11 bits */ 
			*dest++ = 0xc0 + (*source >> 6);
			*dest++ = 0x80 + (*source & 0x3f);
			destLength -= 2;
		} else if (*source < 0x10000) {	/* 16 bits */
			*dest++ = 0xe0 + (*source >> 12);
			*dest++ = 0x80 + ((*source >> 6) & 0x3f);
			*dest++ = 0x80 + (*source & 0x3f);
			destLength -= 3;
		} else if (*source < 0x200000) {	/* 21 bits */
			*dest++ = 0xf0 + (*source >> 18);
			*dest++ = 0x80 + ((*source >> 12) & 0x3f);
			*dest++ = 0x80 + ((*source >> 6) & 0x3f);
			*dest++ = 0x80 + (*source & 0x3f);
			destLength -= 4;
		} else if (*source < 0x4000000)	{ /* 26 bits */
			*dest++ = 0xf8 + (*source >> 24);
			*dest++ = 0x80 + ((*source >> 18) & 0x3f);
			*dest++ = 0x80 + ((*source >> 12) & 0x3f);
			*dest++ = 0x80 + ((*source >> 6) & 0x3f);
			*dest++ = 0x80 + (*source & 0x3f);
			destLength -= 5;
		} else { /* 31 bits */
			*dest++ = 0xfc + (*source >> 30);
			*dest++ = 0x80 + ((*source >> 24) & 0x3f);
			*dest++ = 0x80 + ((*source >> 18) & 0x3f);
			*dest++ = 0x80 + ((*source >> 12) & 0x3f);
			*dest++ = 0x80 + ((*source >> 6) & 0x3f);
			*dest++ = 0x80 + (*source & 0x3f);
			destLength -= 6;
		}
		if (destLength < 1) {
			// fprintf(stderr, "Length overrun in wide_utf8\n");
			exit(1);
		}
	} // for
	*dest++ = 0; // terminating null
	return(dest - oldDest);
} // wide_utf8

utf8_t printBuf[LINELEN];

// the returned value is static, so you cannot have two outstanding
utf8_t *makeUTF(const wide_t *source, int sourceLength){
	wide_utf8(printBuf, LINELEN, source, sourceLength);
	return printBuf;
} // makeUTF
