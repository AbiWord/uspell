// uniprops.h
// copyright c 2003 Raphael Finkel.
// license: Gnu Public License.

// routines to manipulate Unicode characters based on their properties

#ifndef UNICODEPROPS_H
#define UNICODEPROPS_H

#include "mytypes.h"
#include "transcribe.h"

int isCombining(wide_t c);
 /* returns 1 if c is a combining character; 0 else. */
int isAlphabetic(wide_t c);
 /* returns 1 if c is alphabetic (Unicode classes ); 0 else. */
void unPrecompose(wide_t *dest, int *destLength, const wide_t *source,
	int sourceLength);
 /* copies source to dest, but all precomposed characters are expanded into
  * their base and combining forms.
  */
void reduce(wide_t *dest, int *destLength, const wide_t *source,
	int sourceLength, class transcriber *transcribePtr);
 /* To "reduce" a word w -> r(w) means to remove all combining characters
  * (typically accent marks), to reduce precomposed letters to their base
  * forms, and to substitute any language-specific transcriptions.  The
  * transcribePtr may be NULL, in which case no transcription is done.
  */
void toUpper(wide_t *dest, const wide_t *source, int sourceLength);
 /* change all chars to upper case.  sourceLength is in wide_t units, not
  * bytes.
  */
wide_t toFinal(wide_t c);
 /* return the final equivalent of the character c.  Only a few characters 
  * have final forms.  For instance, the final form of כ is ך.  If there is no
  * special form, then just return c itself.
  */

#endif // UNICODEPROPS //
