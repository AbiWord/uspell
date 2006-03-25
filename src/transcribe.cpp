// transcriber class
// copyright c 2003 Raphael Finkel.
// license: Gnu Public License.

// This class provides a way to convert Unicode strings to "sounds-like"
// strings, which are language-dependent, based on a set of transcriptions,
// such as "ay ai", which means that "ay" should be transcribed to "ai".
//
// The method is to preprocess the transcriptions into a finite-state automaton
// that can scan and transform a text in linear time.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utf8convert.h"
#include "transcribe.h"

/* We use a tree-like data structure to describe transcriptions.  Each node has
 * 256 children (typically nil, indicated by 0).  Each node represents what to
 * do if an input string contains the subsequence indicated from the root to
 * the current node.  If the subsequence continues, we go to the appropriate
 * child.  If not, the node contains a replacement string that should
 * substitute for the input string.
 */

transcriber::transcriber(const char *fileName) {
	memset(reinterpret_cast<void *>(&transcribeStart), 0, sizeof(transcribeStart));
	FILE *transcriptionFile;
	if (!*fileName) return; // we can do without a transcription file.
	transcriptionFile = fopen(fileName, "r");
	utf8_t buf[BUFLEN];
	utf8_t *divide;
	if (transcriptionFile == NULL) {
		perror(fileName);
		exit(1);
	}
	while (fgets(reinterpret_cast<char *>(buf), BUFLEN, transcriptionFile)) { // one spec
		buf[strlen(reinterpret_cast<char *>(buf))-1] = 0; // chomp \n
		if (*buf == 0) continue; // empty line
		if (*buf == '#') continue; // comment line
		divide = reinterpret_cast<utf8_t *>(
			strrchr(reinterpret_cast<char *>(buf), ' '));
		if (divide) {
			wide_t *orig, *replace;
			wide_t bigBuf[BUFLEN];
			int origLength, replaceLength;
			*divide++ = 0;
			origLength = utf8_wide(bigBuf, buf, BUFLEN);
			orig = reinterpret_cast<wide_t *>(malloc(origLength*4));
			memcpy(orig, bigBuf, sizeof(wide_t)*origLength);
			replaceLength = utf8_wide(bigBuf, divide, BUFLEN);
			replace = reinterpret_cast<wide_t *>(malloc(replaceLength*4));
			memcpy(replace, bigBuf, sizeof(wide_t)*replaceLength);
			addTranscribe(orig, origLength, replace, replaceLength);
		} else {
			fprintf(stderr, "Bad transcription file line: %s\n", buf);
		}
	} // one spec
	fclose(transcriptionFile);
} // transcriber

void transcriber::recursiveFree(transcribe_t *aNode) {
	int index;
	for (index = 0; index < 256; index += 1) {
		if (aNode->nextState[index]) {
			recursiveFree(aNode->nextState[index]);
			free(aNode->nextState[index]);
		}
	}
} // recursiveFree

transcriber::~transcriber() { // deallocator
	// fprintf(stdout, "deallocating transcriber\n");
	recursiveFree(&transcribeStart);
} // deallocator

void transcriber::addTranscribe(const wide_t* orig, int origLength,
		const wide_t* replace, const int replaceLength) {
	transcribe_t *current;
	const char *sourcePtr;
	int index;
	unsigned int count;
	// fprintf(stdout, "adding transcription %s ", makeUTF((wide_t *) orig,
	// 	origLength));
	// fprintf(stdout, "-> %s\n", makeUTF((wide_t *) replace, replaceLength));
	current = &transcribeStart;
	for (count = 0, sourcePtr = reinterpret_cast<const char *>(orig);
			count < sizeof(wide_t)*origLength; count += 1, sourcePtr++)
	{ // one char of orig
		index = *sourcePtr & 0xff;
		// fprintf(stdout, " %x ", index);
		if (!(current->nextState[index])) {
			current->nextState[index] =
				reinterpret_cast<transcribe_t *>(
					calloc(sizeof(transcribe_t), 1));
			// fprintf(stdout, " ! ");
		}
		current = current->nextState[index];
	} // one char of orig
	// fprintf(stdout, "\n");
	if (current->length) {
		fprintf(stdout, "conflict; %s already ",
			makeUTF(reinterpret_cast<const wide_t *>(orig), origLength));
		fprintf(stdout, " -> %s",
			makeUTF(reinterpret_cast<const wide_t *>(current->replacement),
			current->length));
		fprintf(stdout, ", not %s\n",
			makeUTF(reinterpret_cast<const wide_t *>(replace), replaceLength));
		return;
	}
	current->replacement = replace;
	current->length = replaceLength;
} // addTranscribe

// transcribe the source into dest.  We assume there is room.  Return the
// length of the result.
void transcriber::transcribe(wide_t *dest, int *destLength,
		const wide_t *source, const int sourceLength) {
	// although the parameters (and lengths!) are in wide_t, we work a byte at
	// a time.
	transcribe_t *current;
	const unsigned char *aftSource, *foreSource;
	wide_t *destPtr;
	int index;
	const wide_t *replacePtr;
	current = &transcribeStart;
	aftSource = reinterpret_cast<const unsigned char *>(source);
	foreSource = reinterpret_cast<const unsigned char *>(source);
	destPtr = dest;
	while ((reinterpret_cast<const wide_t *>(foreSource)) <
			(reinterpret_cast<const wide_t *>(source))+sourceLength) {
		// one char of source
		index = *foreSource & 0xff;
		if ((current->nextState[index])) { // the path continues
			current = current->nextState[index];
			foreSource++;
		} else if (current->length) { // end of path; replace
			int count;
			for (count = 0, replacePtr = current->replacement;
					count < current->length; replacePtr++, count += 1) {
				*destPtr++ = *replacePtr;
			}
			aftSource = foreSource;
			current = &transcribeStart;
		} else { // no replacement; output one byte and rescan
			char * foo = reinterpret_cast<char *>(destPtr);
			*(foo)++ = *aftSource++;
			foreSource = aftSource;
			current = &transcribeStart;
		}
	} // one char of source
	// close loose ends
	if (current->length) {
		int count;
		for (count = 0, replacePtr = current->replacement;
				count < current->length; replacePtr++, count += 1) {
			*destPtr++ = *replacePtr;
		}
	} else {
		for (replacePtr = reinterpret_cast<const wide_t *>(aftSource);
			replacePtr < reinterpret_cast<const wide_t *>(foreSource);
			replacePtr++) {
		*destPtr++ = *replacePtr;
	}
}
*destLength = destPtr - dest;
} // transcribe
