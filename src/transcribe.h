// transcribe.h
// copyright c 2003 Raphael Finkel.
// license: Gnu Public License.

#ifndef TRANSCRIBE_H
#define TRANSCRIBE_H

#include "myparameters.h"
#include "mytypes.h"

class transcriber {
	public:
		transcriber(const char *fileName); // initializer
			// It is acceptable to pass a NULL file name, in which case
			// transcribing has no effect.  Otherwise, the file should have one
			// line per transcription, with a single space character in the
			// middle, and the left and right string both in UTF-8.
			// Transcribing a string converts all occurrences of any left
			// string into its associated right string.
		~transcriber(); // deallocator
		void transcribe(wide_t *dest, int *destLength, const wide_t *source,
			const int sourceLength);
			// the lengths are in wide_t units, not bytes.
	private:
	// types
		typedef struct transcribeStruct {
			struct transcribeStruct *nextState[256];
			const wide_t *replacement;
			int length; // (in wide_t units) if not zero, there is a replacement
				// here.
		} transcribe_t;
	// vars
		transcribe_t transcribeStart;
	// methods
		void addTranscribe(const wide_t* orig, int origLength,
		const wide_t* replace, const int replaceLength);
		void recursiveFree(transcribe_t *aNode);
}; // transcriber

#endif // TRANSCRIBE
