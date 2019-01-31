#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kak/kcommon.h>
#include <kak/eprintf.h>
#include <kak/klist.h>
#include <kak/khash.h>
#include "txt.h"
#include "tfile.h"

/* TODO: pass a optional parameter to name the vocab and docid
 * files? */
int main(int argc, char *argv[])
{
	TFile *tfile;
	FILE  *fplog, *fpv, *fpd;
	Token *t;

	esetprogname(estrdup(argv[0]));
	fplog = fopen(strcat(estrdup(argv[0]), "-error.log"), "w");
	esetstream(fplog);

	tfile = newTFile();
	
	readTFile(stdin, tfile);
	printTFile(tfile, stdout);

	fpv = fopen("vocab.txt", "w");
	for (int i = 0; i < tfile->ht->n; i++) {
		for (Node *np = tfile->ht->tab[i]; np != NULL; np = np->next) {
			t = (Token *)(np->data);
			fprintf(fpv, "%s %u\n", t->str, t->id);
		}
	}
	fflush(fpv); fclose(fpv);

	fpd = fopen("docid.txt", "w");
	for (int i = 0; i < tfile->hd->n; i++) {
		for (Node *np = tfile->hd->tab[i]; np != NULL; np = np->next) {
			t = (Token *)(np->data);
			fprintf(fpd, "%s %u\n", t->str, t->id);
		}
	}
	fflush(fpd); fclose(fpd);

	freeTFile(tfile);
	
	fclose(fplog);
	return 0;
}
