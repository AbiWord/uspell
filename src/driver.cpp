// driver.cpp: show how to use the uspell package.
// Usage: driver wordfile transcribefile samplefile supplementalfile
//
//	wordfile is a dictionary file; each word terminated by \n.
//	transcribefile is a file of "sounds like" for helping find
//		close-sounding suggestions for misspelled words.
//	samplefile is a file of words to check spelling of, one per line.
//	supplementalfile is an additional dictionary file (like a personal one)
//
//	all files must be encoded in utf8.
//
// Output: words in samplefile are categorized:
// 	ok: spelled correctly
// 	ok once precomposed letters expanded: mañana should be mañana.
// 	bad; a list of replacements is suggested.
//
// copyright c 2003 Raphael Finkel.
// license: Gnu Public License.

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "uspell.h"
#include "utf8convert.h"
#include "uniprops.h"

#define MAXALTERNATIVE 4

class uSpell *mySpeller;

void treat(utf8_t *word) {
	// see if the word is spelled right, and if not, print alternatives.
	int length;
	wide_t bigBuf[BUFLEN];
	word[strlen(reinterpret_cast<char *>(word)) -1] = 0; // chomp \n
	length = utf8_wide(bigBuf, word, BUFLEN);
	if (mySpeller->isSpelledRight(bigBuf, length)) {
		fprintf(stdout, "%s is ok\n", reinterpret_cast<char *>(word));
	} else { // spelled wrong
		wide_t upperBuf[BUFLEN];
		toUpper(upperBuf, bigBuf, length);
		if (mySpeller->isSpelledRight(upperBuf, length)) {
			// fprintf(stdout, "%s is ok once converted to upper case\n",
			// 	(char *) word);
			return;
		}
		unPrecompose(bigBuf, &length, upperBuf, length);
		if (mySpeller->isSpelledRight(bigBuf, length)) {
			fprintf(stdout, "%s is ok once precomposed letters expanded\n",
				reinterpret_cast<char *>(word));
			return;
		}
		int splitLength = mySpeller->isSpelledRightMultiple(bigBuf, length);
		if (splitLength) {
			fprintf(stdout, "%s is ok as two words with %d, %d chars\n",
				reinterpret_cast<char *>(word), splitLength,
				length-splitLength);
			return;
		}
		fprintf(stdout, "%s -> ", reinterpret_cast<char *>(word));
		utf8_t **list = reinterpret_cast<utf8_t **>(
			calloc(sizeof(utf8_t *), MAXALTERNATIVE));
		int numAlternatives = mySpeller->showAlternatives(bigBuf, length,
			list, MAXALTERNATIVE);
		int index;
		for (index = 0; index < numAlternatives; index++) {
			fprintf(stdout, "%s ", reinterpret_cast<char *>(list[index]));
			free (list[index]);
		}
		fprintf(stdout, "\n");
		// mySpeller->ignoreBadWord(bigBuf, length); // avoid complaining again
		mySpeller->acceptWord(word); // avoid complaining again, take as good
	} // spelled wrong
} // treat

int main(int argc, char *argv[]) {
	if (argc != 5) {
		fprintf(stdout,
			"Usage: %s wordfile transcribefile samplefile supplementalfile\n",
			argv[0]);
		exit(1);
	}
	mySpeller = new uSpell(argv[1], argv[2], uSpell::expandPrecomposed);
	if (*argv[4] && !mySpeller->assimilateFile(fopen(argv[4], "r"))) {
		fprintf(stdout, "Failed to assimilate secondary file\n");
		exit(1);
	}
	FILE *samples = fopen(argv[3], "r");
	if (samples == NULL) {
		perror(argv[3]);
		exit(1);
	}
	utf8_t buf[BUFLEN];
	while (fgets(reinterpret_cast<char *>(buf), BUFLEN, samples)) {
		// one sample word
		treat(buf);
	} // one sample word
	fprintf(stdout, "deallocating mySpeller\n");
	mySpeller->~uSpell();
	// sleep(10);
	return(0);
} // main
