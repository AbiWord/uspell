// uspell.h
// copyright c 2003 Raphael Finkel.
// license: Gnu Public License.
//
// language-specific interface to a dictionary

#ifndef USPELL_H
#define USPELL_H

#include <ctype.h>
#include <stdio.h>
#include "myparameters.h"
#include "mytypes.h"

class uSpell {
	public:
	// constants
		// flags for initializer
		static const char expandPrecomposed = 1<<0;
			// if set, precomposed characters in the dictionary file will be
			// expanded to ordinary unicode characters as the dictionary is
			// loaded.
		static const char upperLower = 1<<1;
			// if set, the language distinguishes case.  This information is
			// not used directly by uspell, but it can guide the application
			// that uses uspell.
		static const char hasCompounds = 1<<2;
			// if set, the language allows compounds of two words.  This
			// information is not used directly by uspell, but it can guide the
			// application that uses uspell.
		static const char hasComposition = 1<<3;
			// if set, the language uses Unicode composing characters that
			// might have precomposed versions.  This information is not used
			// directly by uspell, but it can guide the application that uses
			// uspell.
#		define NUMDICTFILES 7
			// number of open dictionary files per uspell object
			// The last one is reserved, so one fewer is actually allowed
			// Number 1 is the regular dictionary, 2 .. NUMDICTFILES-1 are
			// supplemental.
	// variables
		char theFlags; // should be read-only to applications
	// exceptions
		static const int noSuchFile = 1; // can't open the dictFile
		static const int noMem = 2; // out of memory
		static const int fileOpen = 3; // some file can't be opened
	// procedures
		uSpell(const char *dictFile, const char *transcriptionFile,
			const char flags);
			// initializer.  The dictFile is a newline-delimited list of
			// utf8-encoded words of the language.  It should be the "main
			// file" of the language; its length determines the length of
			// various hash tables.
			// If flags contains expandPrecomposed, then precomposed characters
			// in the languageFile will be expanded, and we will only count the
			// expanded form as properly spelled.  It is about 20% faster to
			// leave this flag off, which has identical behavior if
			// languageFile is fully expanded, with no precomposed characters.
		~uSpell(); // finalizer
		bool assimilateFile(const char* wordFileName);
			// The newFile should be a newline-delimited list of utf8-encoded
			// words of the language.  Returns false if there is a problem,
			// such as too many files (the limit is NUMDICTFILES-1) or a
			// malformed file.
		bool isSpelledRight(const wide_t *string, const int length);
			// length is in wide_t units, not bytes.
		int isSpelledRightMultiple(wide_t *string, const int length);
			// The string is considered spelled right if it is the combination
			// of two words, both spelled right.
			// length is in wide_t units, not bytes.
			// Returns 0 if bad, else the length of the first word.
			// Although not declared with const string, we undo any mods to
			// string before returning.
		void ignoreWord(const utf8_t *string); // null-terminated
			// the given word is now taken as correctly spelled.  However, it
			// will not be given as a suggestion for a misspelling.
		void ignoreWord(const wide_t *string, const int length);
			// length is in wide_t units.
		void acceptWord(const utf8_t *string); // null-terminated
			// the given word is now taken as correctly spelled and can become
			// a suggestion for a misspelling.
		int showAlternatives(const wide_t *probe, const int length,
			utf8_t **list, const int maxAlternatives);
			// returns count of alternative good spellings of 'probe', placed
			// in 'list', not to exceed 'maxAlternatives'.   The alternatives
			// are in newly allocated space; the caller should free() when
			// done.

	private:

	// types
		typedef __uint32_t fileOffset_t;
			// offsetBits of the offset; upper 3 bits are the file number
			// Hash tables hold fileOffset_t values, not strings
	// constants
		static const int maxDistance = 3; // word distance; don't suggest bigger
		static const int maxHashVersion = 5; // number of independent bit hashes
		static const int spread = 2; // difference between words looks for same
			// char within this distance.
		static const int infinity = 100000;
		static const int offsetBits = 29;  // bits used to actually hold offset
		static const fileOffset_t offsetMask = ~(-1 << offsetBits);

	// types
		typedef fileOffset_t *hashTable;
		typedef struct {
			fileOffset_t fileOffset; // offset into wordFile
			int goodness; // distance from proferred spelling; large is bad
		} suggestion_t;

	// variables
		FILE *wordFile;
		hashTable reducedWordTable; // each entry points to wordFile.
		int reducedWordTableLength;
		int reducedWordTableMask;
		hashTable goodWordTable;
			// each good word hashed HASHNUM times to a bit.
		int goodWordTableLength;
		int goodWordTableMask;
		suggestion_t suggestions[BUFLEN]; // kept sorted, best first
		int suggestionCount;
		class transcriber *myTranscribe;
		int fileNumber; // which file we are working on
		FILE *wordFiles[NUMDICTFILES+1]; // wordFile[0] is not used.
			// wordFile[1] is the main dictionary
			// wordFile[1..NUMDICTFILES-1] are read in by assimilateFile();
			// wordFile[NUMDICTFILES] is for accepted but not filed words.
		
	// private routines
		bool inGoodWordTable(const wide_t *string, const int length);
		void insertReducedWordTable(const wide_t *string, const int length,
			const fileOffset_t aValue, const int fileNumber);
		void initSuggestions();
		void addSuggestion(const fileOffset_t fileOffset, const int goodness);
		void addMatches(const wide_t *probe, const int probeLength,
			const wide_t *target, const int targetLength);
		int wordDiff(const wide_t *string1, const int string1Length,
			const wide_t *string2, const int string2Length);
		void uSpell::acceptGoodWord(const utf8_t *buf, int wordPosition,
			int fileNumber);
}; // class uSpell

#endif /* USPELL_H */

