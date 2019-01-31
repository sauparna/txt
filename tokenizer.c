#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <kak/eprintf.h>
#include <kak/kcommon.h>
#include <kak/klist.h>
#include <kak/khash.h>
#include "txt.h"
#include "stemmer.h"
#include "tokenizer.h"

void reset(Stack *stk, int n)
{
	stk->p = -1;
	stk->n = n;
}

void push(Stack *stk, char *c)
{
	int i;
	if (stk->p == (stk->n - 1)) {
		for (i = 0; i < stk->p; i++)
			stk->s[i] = stk->s[i+1];
		stk->p--;
	}
	stk->s[++stk->p] = *c;
}

char pop(Stack *stk)
{
	if (stk->p >= 0) {
		stk->p--;
		return stk->s[stk->p + 1];
	}
	return 0;
}

void printstack(Stack *stk)
{
	int i;
	for (i = 0; i <= stk->p; i++)
		printf("%d:%d ", i, stk->s[i]);
	printf("\n");
}

Tokenizer *newtokenizer(char *delimiters, int casechange, Stack *mem)
{
	Tokenizer *t;
	int i;
	char *c;

	t = (Tokenizer *)emalloc(sizeof(Tokenizer));

	strcpy(t->delimiters, delimiters);
	memset(t->asciitab, SEPCHAR, sizeof(int) * ASCII);

	/* ASCII chars 0 - 31, 127 and those from the delimiter string
	 * are marked as separators and the rest as printable
	 * chars.  */
	t->asciitab[127] = SEPCHAR;	
	for (i = 0; i <= 31; i++)
		t->asciitab[i] = SEPCHAR;
	for (i = 32; i <= 126; i++)
		t->asciitab[i] = PRINTCHAR;
	for (c = t->delimiters; *c; c++)
		t->asciitab[(int)(*c)] = SEPCHAR;
	t->mem = mem;
	t->casechange = casechange;
	return t;
}

/* TODO: what could be done for portability ? */

uint32_t gettoken(Token *tok, FILE *fp, Tokenizer *t)
{
	uint32_t l, type;
	char c, *s, c_[2];
	
	c      = ' ';
	l      = 0;
	type   = TERM;
	s      = tok->str;
	*s     = '\0';
	while (fread(&c, 1, 1, fp) != 0) {
		if (t->asciitab[(uint32_t)c] == SEPCHAR) {
			push(t->mem, &c);
			if (l == 0)
				continue;
			*s = '\0';
			if (c == '>') {
				pop(t->mem);
				c_[1] = pop(t->mem);
				c_[0] = pop(t->mem);
				if (c_[1] == '<')
					type = OTAG;
				else if (c_[1] == '/' && c_[0] == '<')
					type = CTAG;
			}
			tok->l = l;
			return type;
		}
		if (t->casechange == LOWERCASE && c >= 65 && c <= 90)
			c += 32;
		*s = c;
		s++;
		l++;
	}
	if (ferror(fp))
		eprintf("In gettoken(), fread() failed\n");
	return 0;
}

/* Parser */


Parser *newparser(char type, char **cw, int c, int n, int x_min, int x_max)
{
	Node *npterm_;
	Parser *p;
	
	p = (Parser *)emalloc(sizeof(Parser));
	p->type = type;

	if (type == 'd') {
		p->n_tag = 0;
		strcpy(p->doctag, "doc");
		strcpy(p->idtag,  "docno");
	}
	else if (type == 'q') {
		p->n_tag = 0;
		strcpy(p->doctag, "top");
		strcpy(p->idtag,  "num");
	}
	else
		eprintf("newparser(): unknown parser\n");
	
	p->c = 0; p->hcw     = NULL;
	p->n = 0; p->stemmer = NULL;
	p->x_min = MINTERMLEN;
	p->x_max = MAXTERMLEN;
	
	if (c) {
		p->c = c;
		p->hcw = newhash(NHASH, _strcmp, _strhash);
		int ncw;
		sscanf(cw[0], "%d", &ncw);
		for (int i = 1; i <= ncw; i++) {
			npterm_ = newnode(cw[i]);
			hlookup(p->hcw, npterm_, 1);
			/* assuming incoming common-words are unique;
			 * not checking for repeatations. */
		}
	}
	if (n) {
		p->n = n;
		p->stemmer = create_stemmer();
	}
	if (x_min)
		p->x_min = x_min;
	if (x_max)
		p->x_max = x_max;
	
	return p;
}

void freeparser(Parser *p)
{
	if (p == NULL)
		return;
	for (int i = 0; i < p->hcw->n; i++)
		freelist(p->hcw->tab[i], NULL);
	free_stemmer(p->stemmer);
	free(p);
}

void __print_doc_buf(char *buf, uint32_t b)
{
	uint32_t i, c;
	char *bp;
	c = 0;
	fprintf(stderr, "%s: ", buf);
	for (bp = buf+MAXTERMLEN+1, i = MAXTERMLEN+1; i < b; i++, bp++) {
		if (*bp == '\0') {
			fprintf(stderr, " ");
			c++;
		}
		else
			fprintf(stderr, "%c", *bp);
	}
	fprintf(stderr, "\nToken count = %d\n", c);
}

uint32_t getdoc(char **buf, Parser *parser, FILE *fp)
{
	/* Return byte-size of a buffer, whose first MAXTERMLEN+1
	 * bytes contain the NULL-terminated document ID and the rest
	 * of it contain NULL-terminated term strings. */
	
	uint32_t  size, indoc, b, type;
	Tokenizer *toktxt, *tokid;
	Token     t;
	char      *septxt = " ;,.:`'\"?!(){}[]<>~^&*_-+=#$%@|\\/";
	char      *sepid  = " <>";
	Stack     mem;

	reset(&mem, 3);

	toktxt = newtokenizer(septxt, LOWERCASE, &mem);
	tokid  = newtokenizer(sepid, KEEPCASE, &mem);

	size   = MB;
	b      = MAXTERMLEN + 1;
	*buf   = (char *)emalloc(size);
	
	t.str  = *buf + b;
	t.l    = 0;
	
	indoc  = 0;
		
	while((type = gettoken(&t, fp, toktxt)) != 0) {
		if ((type == OTAG) && (strcmp((char *)t.str, parser->doctag) == 0)) {
			indoc = 1;
			continue;
		}
		if (!indoc)
			continue;
		if ((type == OTAG) && (strcmp((char *)t.str, parser->idtag) == 0)) {
			gettoken(&t, fp, tokid);
			memcpy(*buf, t.str, t.l + 1);
			continue;
		}
		if (type == TERM) {
			if (size < (b + t.l + 1)) {
				while ((size <<= 1) < (b + t.l + 1))
					;
				*buf = erealloc(*buf, size);
				t.str = *buf + b;
			}
			t.str += t.l + 1;
			b += t.l + 1;
			continue;
		}
		if ((type == CTAG) && (strcmp((char *)t.str, parser->doctag) == 0))
			return b;
	}
	free(*buf);
	return 0;
}
