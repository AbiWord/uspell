// uspell.cpp
// copyright c 2003 Raphael Finkel.
// license: Gnu Public License.
//
// The uSpell class provides a language-specific interface to a dictionary.
// Methods:
// 	uSpell: initializes, reads a dictionary and optional transcription file (for
// 		"sounds-like" substitutions)
// 	~uSpell: finalizes, deallocates the (fairly large) data structures
// 	assimilateFile: incorporates another dictionary file, such as a personal
// 		dictionary.
//	isSpelledRight: tells if a given word is found in the dictionary.
//	isSpelledRightMultiple: tells if a given word is found in the dictionary,
//		possibly by decomposing it into two words, both spelled right.
//	ignoreWord: adds word to the dictionary, but not as a possible suggestion
//		for misspelled words.
//	acceptWord: adds word to the dictionary and as a possible suggestion for
//		misspelled words.
//	showAlternatives: lists all close alternatives to a given misspelled word
//
//	All words are represented in Unicode.  Most routines use UCS; some also
//	accept UTF8.  The dictionary files must be in UTF8.

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "uspell.h"
#include "utf8convert.h"
#include "uniprops.h"
#include "transcribe.h"
#include "lookup2.h"

void uSpell::ignoreWord(const wide_t *string, const int length) {
	int hashVersion;
	unsigned int hashValue;
	for (hashVersion = 1; hashVersion <= maxHashVersion; hashVersion++) {
		hashValue = hash2(string, length, hashVersion) &
			goodWordTableMask;
		goodWordTable[hashValue >> 5] |= 1 << (hashValue & 0x1f);
	}
} // ignoreWord

bool uSpell::inGoodWordTable(const wide_t *string, const int length) {
	int hashVersion;
	int hashValue;
	int loc, offset;
	for (hashVersion = 1; hashVersion <= maxHashVersion; hashVersion++) {
		hashValue = hash2(string, length, hashVersion) &
			goodWordTableMask;
		loc = hashValue >> 5;
		offset = 1 << (hashValue & 0x1f);
		// fprintf(stdout, "hashvalue 0x%x at table[%d], bit 0x%x ... ", hashValue, loc, offset);
		if (goodWordTable[loc] & offset) {
			// fprintf(stdout, "found\n");
		} else {
			// fprintf(stdout, "not found\n");
			return(false);
		}
	}
	return(true); // found
} // inGoodWordTable

// We use quadratic rehashing to avoid mallocs for external chains.
// We don't store the keys in the table, just a single fileOffset_t datum.

int insertCount = 0;

void uSpell::insertReducedWordTable(const wide_t *string, const int length,
		const fileOffset_t aValue, const int fileNumber) {
	int hashVal = hash2(string, length, 1) & reducedWordTableMask;
	int probeDelta = 1;
	int pathLength = 0;
	while (reducedWordTable[hashVal]) {
		if (reducedWordTable[hashVal] & offsetMask == aValue) return;
			// duplicate
		probeDelta = (probeDelta<<1) | 1;
		hashVal = (hashVal + probeDelta) & reducedWordTableMask;
		pathLength += 1;
	}
	reducedWordTable[hashVal] = aValue + (fileNumber << offsetBits);
	insertCount += 1;
	if (pathLength > 100) { // too long!
		fprintf(stdout, "You need a bigger hash table\n");
		exit(1);
	}
	// fprintf(stdout, "inserting %s=%d at location %d\n",
	// 	makeUTF(string, length), aValue, hashVal);
} // insertReducedWordTable

void uSpell::initSuggestions() {
	suggestions[0].goodness = infinity; // pseudo-data
	suggestionCount = 1;
} // initSuggestions

void uSpell::addSuggestion(const fileOffset_t fileOffset, const int goodness) {
	int index;
	suggestion_t tmpSuggestion, nextSuggestion;
	if (goodness > maxDistance) return; // not good enough
	if (suggestionCount >= BUFLEN) { // should toss the bottom one
		fprintf(stderr, "Out of room for suggestions\n");
		exit(1);
	}
	// skip better and equally good suggestions; we use better heuristics first
	for (index = 0; goodness >= suggestions[index].goodness; index++){
		if (suggestions[index].fileOffset == fileOffset) { // duplicate
			if (suggestions[index].goodness > goodness)
				suggestions[index].goodness = goodness; // take better one
			return;
		} // duplicate
	}
	// fprintf(stdout, "adding suggestion %d (%d)\n", fileOffset, goodness);
	// put this suggestion in, saving what it will overwrite
	tmpSuggestion = suggestions[index];
	suggestions[index].fileOffset = fileOffset;
	suggestions[index].goodness = goodness;
	// move suggestions[index .. suggestionCount-1] over.
	for (index += 1; index <= suggestionCount; index++) {
		nextSuggestion = suggestions[index];
		suggestions[index] = tmpSuggestion;
		tmpSuggestion = nextSuggestion;
	}
	suggestionCount += 1;
} // addSuggestion

// add all the words in the reducedWordTable that match the given probe to
// suggestions[].  The probe should already be reduced.
void uSpell::addMatches(const wide_t *probe, const int probeLength,
		const wide_t *target, const int targetLength) {
	int hashVal;
	int probeDelta = 1;
	fileOffset_t index;
	hashVal = hash2(probe, probeLength, 1) & reducedWordTableMask;
	while ((index = reducedWordTable[hashVal])) {
		// we never store a 0, because the first file is #1, not #0.
		int fileNumber = index >> offsetBits;
		fileOffset_t offset = index & offsetMask;
		utf8_t wordBuf[BUFLEN]; int wordLen;
		wide_t reduceBuf[BUFLEN]; int reduceLen;
		wide_t bigWordBuf[BUFLEN];
		fseek(wordFiles[fileNumber], offset, SEEK_SET);
		fgets(reinterpret_cast<char *>(wordBuf), BUFLEN, wordFiles[fileNumber]);
		wordBuf[strlen(reinterpret_cast<char *>(wordBuf))-1] = 0; // chomp \n
		wordLen = utf8_wide(bigWordBuf, wordBuf, BUFLEN);
		reduce(reduceBuf, &reduceLen, bigWordBuf, wordLen, myTranscribe);
		addSuggestion(index, wordDiff(reduceBuf, reduceLen, target,
			targetLength));
		// fprintf(stdout, "match %s", makeUTF(reduceBuf, reduceLen));
		// fprintf(stdout, "/%s(%d) ", makeUTF(target, targetLength),
		// 	wordDiff(reduceBuf, reduceLen, target, targetLength));
		probeDelta = (probeDelta<<1) | 1;
		hashVal = (hashVal + probeDelta) & reducedWordTableMask;
	}
} // addMatches

// probe is not yet reduced; it is misspelled.  Print all the words that it
// might be.
int uSpell::showAlternatives(const wide_t *probe, const int length,
	utf8_t **list, const int maxAlternatives) {
	wide_t reduceBuf[BUFLEN];
	int reduceLength;
	// fprintf(stdout, "checking %s\n", makeUTF(probe, length));
	if (inGoodWordTable(probe, length)) {
		// fprintf(stdout, "spelled correctly\n");
		return(0);
	}
	initSuggestions();
	reduce(reduceBuf, &reduceLength, probe, length, myTranscribe);
	// fprintf(stdout, "(reduction %s) ", makeUTF(reduceBuf, reduceLength));
	addMatches(reduceBuf, reduceLength, reduceBuf, reduceLength);
	{ // omit seriatim each letter of the reduction.
		wide_t tmp[BUFLEN];
		wide_t save1, save2;
		int index;
		memcpy(tmp, reduceBuf, sizeof(wide_t)*reduceLength);
		save2 = *tmp;
		for (index = 0; index < reduceLength; index++) { // omit letter at index
			save1 = tmp[index];
			tmp[index] = save2;
			save2 = save1;
			// fprintf(stdout, "(omission %s) ", makeUTF(tmp+1, reduceLength-1));
			addMatches(tmp+1, reduceLength-1, reduceBuf, reduceLength);
		}
	} // omit seriatim
	{ // interchange seriatim each letter of the reduction.
		int index;
		wide_t tmp[BUFLEN];
		memcpy(tmp, reduceBuf, sizeof(wide_t)*reduceLength);
		for (index = 1; index < reduceLength; index++) {
			wide_t saveChar;
			saveChar = tmp[index];
			tmp[index] = tmp[index-1];
			tmp[index-1] = saveChar;
			// fprintf(stdout, "(interch %s) ", makeUTF(tmp, reduceLength));
			addMatches(tmp, reduceLength, reduceBuf, reduceLength);
			tmp[index-1] = tmp[index];
			tmp[index] = saveChar;
		}
	} // interchange seriatim
	// fprintf(stdout, "\n");
	int index;
	for (index = 0; index < suggestionCount-1 /* last is pseudo */; index++) {
		utf8_t buf[BUFLEN];
		fileOffset_t offset;
		int fileNumber;
		if (index >= maxAlternatives) break;
		offset = suggestions[index].fileOffset & offsetMask;
		fileNumber = suggestions[index].fileOffset >> offsetBits;
		fseek(wordFiles[fileNumber], offset, SEEK_SET);
		fgets(reinterpret_cast<char *>(buf), BUFLEN, wordFiles[fileNumber]);
		buf[strlen(reinterpret_cast<char *>(buf))-1] = 0; // chomp \n
		list[index] = reinterpret_cast<utf8_t *>(
			malloc(strlen(reinterpret_cast<char *>(buf))+1));
		strcpy(reinterpret_cast<char *>((list[index])),
			reinterpret_cast<char *>(buf));
	}
	return(index);
} // showAlternatives

void inline uSpell::acceptGoodWord(const utf8_t *buf, int wordPosition,
		int fileNumber) {
	wide_t bigBuf1[BUFLEN], bigBuf2[BUFLEN], reduceBuf[BUFLEN];
	int bigLength, reduceLength;
	bigLength = utf8_wide(bigBuf1, buf, BUFLEN);
	if (theFlags & expandPrecomposed) {
		unPrecompose(bigBuf2, &bigLength, bigBuf1, bigLength);
		if (inGoodWordTable(bigBuf2, bigLength))
			return; // no need for duplicate
		ignoreWord(bigBuf2, bigLength); // actually a good word
		reduce(reduceBuf, &reduceLength, bigBuf2, bigLength, myTranscribe);
	} else { // don't expand precomposed
		if (inGoodWordTable(bigBuf1, bigLength))
			return; // no need for duplicate
		ignoreWord(bigBuf1, bigLength); // actually a good word
		reduce(reduceBuf, &reduceLength, bigBuf1, bigLength, myTranscribe);
	}
	// fprintf(stdout, "for reduced form [%s]",
	// 		makeUTF(bigBuf2, bigLength));
	//	fprintf(stdout, "->[%s]\n", makeUTF(reduceBuf, reduceLength));
	insertReducedWordTable(reduceBuf, reduceLength, wordPosition,
		fileNumber);
	{ // omit seriatim each letter of the reduction.
		wide_t tmp[BUFLEN];
		wide_t save1, save2;
		int index;
		memcpy(tmp, reduceBuf, sizeof(wide_t)*reduceLength);
		save2 = *tmp;
		for (index = 0; index < reduceLength; index++) {
			// omit letter at index
			save1 = tmp[index];
			tmp[index] = save2;
			save2 = save1;
			insertReducedWordTable(tmp+1, reduceLength-1, wordPosition,
				fileNumber);
		}
	} // omit seriatim
} // acceptGoodWord

void uSpell::acceptWord(const utf8_t *string) {
	int wordPosition;
	if (wordFiles[NUMDICTFILES] == NULL) { // first time; create the file
		char *origTemplate = "/tmp/uspell.XXXXXX"; 
		char fileName[BUFLEN];
		strncpy(fileName, origTemplate, strlen(origTemplate)+1);
		int fd = mkstemp(fileName);
		// fprintf(stdout, "Temp file name from template %s is %s\n",
		// 	origTemplate, fileName);
		wordFiles[NUMDICTFILES] = fdopen(fd, "w+");
		if (wordFiles[NUMDICTFILES] == NULL) throw(fileOpen);
		unlink(fileName);  // a very temp file; open only so long as we last
		wordPosition = 0;
	} else {
		fseek(wordFiles[NUMDICTFILES], 0L, SEEK_END);
		wordPosition = ftell(wordFiles[NUMDICTFILES]);
	}
	fwrite(string, 1, strlen(reinterpret_cast<const char *>(string)),
		wordFiles[NUMDICTFILES]);
	fwrite("\n", 1, 2, wordFiles[NUMDICTFILES]);
	acceptGoodWord(string, wordPosition, NUMDICTFILES);
} // acceptWord

bool uSpell::assimilateFile(const char *wordFileName) {
	int wordCount;
	int wordPosition;
	FILE *wordFile = fopen(wordFileName, "r");
	utf8_t buf[BUFLEN];
	if (wordFile == NULL) return(0);
	// fprintf(stdout, "assimilating file\n");
	fileNumber += 1;
	wordFiles[fileNumber] = wordFile;
	if (fileNumber >= NUMDICTFILES) return(false);
	// populate the hash table
	if (fseek(wordFile, 0L, SEEK_SET)) { // make sure at start
		return(false); // can't fseek
	}
	wordPosition = 0;
	for (wordCount = 0;
			fgets(reinterpret_cast<char *>(buf), BUFLEN, wordFile);
			wordCount += 1) {
		if (!strlen(reinterpret_cast<char *>(buf))) break;
		buf[strlen(reinterpret_cast<char *>(buf))-1] = 0; // chomp \n
		acceptGoodWord(buf, wordPosition, fileNumber);
		wordPosition = ftell(wordFile);
	} // one word
	// fprintf(stdout, "Added file %d.  Table density: %d/%d entries (%d%%)\n",
	// 	fileNumber, insertCount, reducedWordTableLength,
	// 	(int) (0.5 + insertCount*100 / reducedWordTableLength));
	return(true);
} // assimilateFile

uSpell::uSpell(const char *dictFile, const char *transcriptionFile,
		const char flags) {
	unsigned int tmpLength;
	theFlags = flags;
	wordFile = fopen(dictFile, "r");
	if (wordFile == NULL) {
		throw(noSuchFile);
	}
	// fprintf(stdout, "starting to assimilate\n");
	myTranscribe = new transcriber(transcriptionFile);
	fseek(wordFile, 0, SEEK_END);
	tmpLength = ftell(wordFile);
	fclose(wordFile);
	reducedWordTableLength = 1;
	while (tmpLength) {
		reducedWordTableLength <<= 1;
		tmpLength >>= 1;
	}
	reducedWordTableMask = reducedWordTableLength - 1;
	fprintf(stdout, "Table length: %d entries\n", reducedWordTableLength);
	reducedWordTable = reinterpret_cast<hashTable>(
		calloc(sizeof(reducedWordTable[0]), reducedWordTableLength));
	if (reducedWordTable == NULL) {
		throw(noMem);
	}
	// good word table has same effective size, but it's a bit table
	goodWordTableLength = reducedWordTableLength >> 5;
	goodWordTableMask = reducedWordTableMask;
	goodWordTable = reinterpret_cast<hashTable>(
		calloc(sizeof(goodWordTable[0]), goodWordTableLength));
	if (goodWordTable == NULL) {
		throw(noMem);
	}
	// initialize all wordfiles
	memset(wordFiles, 0, (NUMDICTFILES+1) * sizeof(wordFiles[0]));
	fileNumber = 0; // assimilateFile will start with file #1.
	assimilateFile(dictFile);
} // uSpell::uSpell

uSpell::~uSpell() {
	int index;
	// fprintf(stdout, "deallocator called\n");
	for (index = 0; index <= NUMDICTFILES; index += 1) {
		if (wordFiles[index] != NULL) fclose(wordFiles[index]);
	}
	free(reinterpret_cast<char *>(reducedWordTable));
	free(reinterpret_cast<char *>(goodWordTable));
	myTranscribe->~transcriber();
} // ~uSpell

void uSpell::ignoreWord(const utf8_t *string) {
	int length;
	wide_t bigBuf[BUFLEN];
	length = utf8_wide(bigBuf, string, BUFLEN);
	ignoreWord(bigBuf, length);
} // ignoreWord

bool uSpell::isSpelledRight(const wide_t *string, const int length) {
	int hashVersion;
	int hashValue;
	int loc, offset;
	for (hashVersion = 1; hashVersion <= maxHashVersion; hashVersion++) {
		hashValue = hash2(string, length, hashVersion) &
			goodWordTableMask;
		loc = hashValue >> 5;
		offset = 1 << (hashValue & 0x1f);
		if (goodWordTable[loc] & offset) {
			// fprintf(stdout, "found with hashversion %d\n", hashVersion);
		} else {
			// fprintf(stdout, "not found with hashVersion %d\n", hashVersion);
			return(0);
		}
	}
	return(1); // found
} // isSpelledRight

int uSpell::isSpelledRightMultiple(wide_t *string, const int length) {
	if (isSpelledRight(string, length)) return(length);
	int divide;
	int finalIndex = -1; // most recent position of final character
	wide_t theOriginal = 0; // avoid warning 
	wide_t lastChar;
	for (divide = 1; divide < length-1; divide += 1) {
		// consider string[0..divide-1] + string[divide..length-1]
		lastChar = *(string+divide-1);
		if (toFinal(lastChar) != lastChar) { // it has a final form
			finalIndex = divide-1;
			theOriginal = lastChar;
			*(string+divide-1) = toFinal(theOriginal);
			// fprintf(stdout, "converting to final, position %d\n", divide-1);
		} else if (!isCombining(lastChar)) {
			if (finalIndex != -1) { // reset old final to original
				// fprintf(stdout, "restoring at position %d\n", finalIndex);
				*(string+finalIndex) = theOriginal;
				finalIndex = -1; // any final character no longer is "last"
			}
		}
		if (isSpelledRight(string, divide) && 
				// fprintf(stdout, "OK at divide %d\n", divide) &&
				isSpelledRight(string+divide, length-divide)) {
			if (finalIndex != -1) { // reset old final to original
				// fprintf(stdout, "restoring at position %d\n", finalIndex);
				*(string+finalIndex) = theOriginal;
			}
			return(divide);
		}
	}
	return(0);
} // isSpelledRightMultiple

// return the sum of the number of letters in each string not in the other.
int uSpell::wordDiff(const wide_t *string1, const int string1Length,
		const wide_t *string2, const int string2Length) {
	int index1, index2, answer;
	wide_t tmp[BUFLEN];
	answer = 0;
	memcpy((void *) tmp, (void *) string2, sizeof(wide_t)*string2Length);
	for (index1 = 0; index1 < string1Length; index1++) {
		// seek string1[index1] nearby in tmp
		answer += 1; // we haven't found it.
		for (index2 = index1-spread; index2 <= index1+spread; index2++) {
			if (index2 < 0) continue; // don't check before start
			if (index2 >= string2Length) break; // or after end of string2
			if (string1[index1] == tmp[index2]) {
				tmp[index2] = 0; // prevent finding again.
				answer -= 1; // we did find it.
				break;
			}
		}
	} // seek string1[index1]
	memcpy((void *) tmp, (void *) string1, sizeof(wide_t)*string1Length);
	for (index2 = 0; index2 < string2Length; index2++) {
		// seek string2[index2] nearby in tmp
		answer += 1; // we haven't found it.
		for (index1 = index2-spread; index1 <= index2+spread; index1++) {
			if (index1 < 0) continue; // don't check before start
			if (index1 >= string1Length) break; // or after end of string1
			if (string2[index2] == tmp[index1]) {
				tmp[index1] = 0; // prevent finding again.
				answer -= 1; // we did find it.
				break;
			}
		}
	} // seek string2[index2]
	return(answer);
} // wordDiff
