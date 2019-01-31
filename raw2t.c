/*
  Tokenizes all content within the <DOC> tags of a TREC document,
  excluding the markup tags, and the document-id.

  Spaces, punctuations and special characters are treated as
  token-separators.

  Letters are converted to lower case.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kak/eprintf.h>
#include <kak/kcommon.h>
#include <kak/klist.h>
#include <kak/khash.h>
#include "txt.h"
#include "tfile.h"
#include "parser.h"
#include "cw.h"

void usage(char *progname)
{
	eprintf("usage: %s [-c] [-n] [-x] <in.txt >out.t\n", progname);
}

int main(int argc, char *argv[])
{
	FILE   *fplog;
	TFile  *tfile;
	Parser *parser;
	int    c, n, x_min, x_max, q, size, bpack;
	
	esetprogname(estrdup(argv[0]));
	fplog = fopen(strcat(estrdup(argv[0]), "-error.log"), "w");
	esetstream(fplog);

	c = n = x_min = x_max = q = bpack = 0;
	size = MB;
	
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0)
			c = 1;
		else if (strcmp(argv[i], "-n") == 0)
			n = 1;
		else if (strcmp(argv[i], "-x") == 0) {
			x_min = 4;
			x_max = 20;
		}
		else {
			/* process non-optional arguments here*/
		}	
	}

	parser = newparser('d', cwSMART, c, n, x_min, x_max);	
	tfile  = parse(parser, stdin);
	writeTFile(stdout, tfile);
	fflush(stdout);
	freeTFile(tfile);
	
	fclose(fplog);
	
	return 0;
}
