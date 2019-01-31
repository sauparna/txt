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
	eprintf("usage: %s [-c] [-n] [-x] [-q corpus.t] <in.txt >out.t\n", progname);
}

int main(int argc, char *argv[])
{
	FILE   *fplog, *vfp;
	TFile  *tfile, *tfile_;
	Hash   *hv;
	Parser *parser;
	char   vfile[KB];
	int    c, n, x_min, x_max, q, size, bpack;
	uint8_t *buf;
	
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
		else if (strcmp(argv[i], "-q") == 0) {
			if (i + 1 <= argc - 1) {
				strcpy(vfile, argv[++i]);
				q = 1;
			}
			else
				usage(argv[0]);
		}
		else {
			/* process non-optional arguments here*/
		}	
	}

	if (q) {
		parser = newparser('q', cwSMART, c, n, x_min, x_max);
		tfile_ = newTFile();
		hv     = newhash(NHASHT, cmptoken_str, hashtoken_str);
		buf    = (uint8_t *)emalloc(size);
		vfp    = fopen(vfile, "r");

		
		if ((n = fread(buf, BTHEADER, 1, vfp)) != 1)
			eprintf("fread() failed on %d byte THeader\n",
				BTHEADER);
		if (unpackTHeader(tfile_->h, BTHEADER, buf) != BTHEADER)
			eprintf("unpackTHeader() failed on %d byte THeader\n",
				BTHEADER);

		bpack = tfile_->h->bt > tfile_->h->bd ? tfile_->h->bt : tfile_->h->bd;
		
		if (size < bpack) {
			while ((size <<= 1) < bpack);
			buf = erealloc(buf, size);
		}
		
		if ((n = fread(buf, tfile_->h->bt, 1, vfp)) != 1)
			eprintf("fread() failed on %d byte tfile->h->bt\n",
				tfile_->h->bt);
		if (unpack_idmap(hv, tfile_->h->bt, buf) != tfile_->h->bt)
			eprintf("unpack_idmap() failed on %d byte tfile->h->bt\n",
				tfile_->h->bt);

		fclose(vfp);
		free(buf);
		
		tfile = parseq(tfile_->h, hv, parser, stdin);

		freeTFile(tfile_);
	}
	else {		
		parser = newparser('d', cwSMART, c, n, x_min, x_max);	
		tfile  = parse(parser, stdin);
	}
	
	send(stdout, tfile); fflush(stdout);
	freeTFile(tfile);
	
	fclose(fplog);
	
	return 0;
}
