#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <kak/eprintf.h>
#include <kak/kcommon.h>
#include <kak/klist.h>
#include <kak/khash.h>
#include "stemmer.h"
#include "crc.h"
#include "tfile.h"
#include "txt.h"
#include "parser.h"

const uint8_t TERM = 0x01;
const uint8_t OTAG = 0x02;
const uint8_t CTAG = 0x03;
static char *str = "width=32 poly=0x04c11db7 init=0x00000000 refin=false refout=false xorout=0xffffffff check=0x765e7680 name=\"CRC-32/POSIX\"";
static char crc_s[128];
static model_t crc_m;

/* Tokenizer */

void reset(Stack *stk, int n)
{
	stk->p = -1;
	stk->n = n;
}

void push(Stack *stk, uint8_t *c)
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

int gettoken(Token *tok, FILE *fp, Tokenizer *t)
{
	uint32_t  l;
	uint8_t c, *s, c_[2];
	
	c      = ' ';
	l      = 0;
	tok->t = TERM;
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
					tok->t = OTAG;
				else if (c_[1] == '/' && c_[0] == '<')
					tok->t = CTAG;
			}
			tok->l = l;
			return 1;
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

static void __initcrc()
{
	int ret;
	strcpy(crc_s, str);
	crc_m.name = NULL;
	ret = read_model(&crc_m, crc_s);
	if (ret == 2)
		eprintf("__initcrc() out of memory\n");
	else if (ret == 1)
		eprintf("__initcrc() %s: unusable model\n",
			crc_m.name == NULL ? "<no name>" : crc_m.name);
	crc_table_wordwise(&crc_m);
}

static void __deinitcrc()
{
	free(crc_m.name);
	crc_m.name = NULL;
}

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

void __print_doc_buf(uint8_t *buf, uint32_t b)
{
	uint32_t i;
	uint8_t *bp;
	fprintf(stderr, "%s.\n", buf);
	for (bp = buf+MAXTERMLEN+1, i = MAXTERMLEN+1; i < b; i++, bp++) {
		if (*bp == '\0')
			fprintf(stderr, " ");
		else
			fprintf(stderr, "%c", *bp);
	}
	fprintf(stderr, ".\n");
}

uint32_t getdoc(uint8_t **buf, Parser *parser, FILE *fp)
{
	/* Return byte-size of a buffer, whose first MAXTERMLEN+1
	 * bytes contain the NULL-terminated document ID and the rest
	 * of it contain NULL-terminated term strings. */
	
	uint32_t  size, indoc, b;
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
	*buf   = (uint8_t *)emalloc(size);
	
	t.str  = *buf + b;
	t.l    = 0;
	t.id   = 0;
	t.t    = 0x00;
	
	indoc  = 0;
		
	while(gettoken(&t, fp, toktxt) == 1) {
		if ((t.t == OTAG) && (strcmp((char *)t.str, parser->doctag) == 0)) {
			indoc = 1;
			continue;
		}
		if (!indoc)
			continue;
		if ((t.t == OTAG) && (strcmp((char *)t.str, parser->idtag) == 0)) {
			gettoken(&t, fp, tokid);
			memcpy(*buf, t.str, t.l + 1);
			continue;
		}
		if (t.t == TERM) {
			if (size < (b + t.l + 1)) {
				while ((size <<= 1) < (b + t.l + 1))
					;
				*buf = erealloc(*buf, size);
				t.str = *buf + b;
			}
			t.str += t.l + 1;
			/* *(t.str)++ = ' '; */
			b += t.l + 1;
			continue;
		}
		if ((t.t == CTAG) && (strcmp((char *)t.str, parser->doctag) == 0)) {
			/* *(t.str) = '\0'; */
			/* b++; */
			return b;
		}
	}
	free(*buf);
	return 0;
}

TFile *parse(Parser *parser, FILE *fp)
{
	uint32_t b, tid, did, i, l, size, stemlen, e, crc;
	uint8_t *buf, s[MAXTERMLEN+1], *bp, docid[MAXTERMLEN+1];
	TFile   *tfile;
	TDoc    *tdoc;
	Token   *t_, t__;
	Node    n__, *npt_, *npt, *nppost, *nppost_;
	Post    *post, *post_;
	double  logtf;
	Hash    *hp;

	t__.str = s;
	t__.l   = 0;
	t__.t   = 0x00;
	t__.id  = 0;

	tid  = 1;
	did  = 1;
	e    = 0;
	size = MB;
	
	__initcrc();
	tfile = newTFile();

	while ((b = getdoc(&buf, parser, fp)) != 0) {

		tdoc = newTDoc();
		hp   = newhash(NHASH, cmppost, hashpost);
		
		/* process the doc id: hash the docid string and
		 * assign a id */

		strcpy((char *)docid, (char *)buf);
		t_   = newtoken(docid, strlen((char *)docid), 0x00, 0);
		npt_ = newnode(t_);
		npt  = hlookup(tfile->hd, npt_, 1);
		if (npt == npt_) { /* new doc, update headers */
			((Token *)(npt->data))->id = did++;
			tdoc->h->id   = ((Token *)(npt->data))->id;
			tfile->h->bd += ((Token *)(npt->data))->l + 1 + 4;
		}
		else
			freenode(npt_, freetoken);
		
		/* process the tokens */
		
		l = 0;

		for(bp = buf+MAXTERMLEN+1, i = MAXTERMLEN+1; i < b; i++, bp++) {
			
			if (*bp == '\0') {
				
				t__.str[l] = *bp;
				t__.l = l;
				l = 0;

				/* drop a short or long token */
				if (t__.l < parser->x_min || t__.l > parser->x_max) 
					continue;
				
				 /* drop too common a token */
				if (parser->c == 1) {
					n__.data = t__.str;
					npt = hlookup(parser->hcw, &n__, 0);
					if (npt != NULL)
						continue;
				}
				
				/* normalize the token */
				if (parser->n == 1) {
					stemlen = stem(parser->stemmer, (char *)t__.str, t__.l - 1);
					t__.str[stemlen + 1] = '\0';
					t__.l = stemlen + 1;
				}

				/* compute document's byte-length */
				tdoc->h->sumb += t__.l;
			
				/* hash the token and assign a id */
				t_   = newtoken(t__.str, t__.l, t__.t, 0);
				npt_ = newnode(t_);
				npt  = hlookup(tfile->ht, npt_, 1);
				if (npt == npt_) {/* new term */
					((Token *)(npt->data))->id = tid++;
					tfile->h->nt++;
					tfile->h->bt += ((Token *)(npt->data))->l + 1 + 4;
				}
				else
					freenode(npt_, freetoken);
				
				/* build Post, hash it to sum the tf */
				post_   = newpost(((Token *)(npt->data))->id, 1);
				nppost_ = newnode(post_);
				nppost  = hlookup(hp, nppost_, 1);
				if (nppost != nppost_) { /* repeating token */
					((Post *)(nppost->data))->tf++;
					freenode(nppost_, free);
				}

				continue;
			}
			
			t__.str[l++] = *bp;
		}

		/* empty the Posts hash 'hp' into TDoc's payload and
		 * header */
			
		for (int j = 0; j < hp->n; j++) {
			for (Node *np = hp->tab[j]; np != NULL; np = np->next) {
				post = (Post *)(np->data);
				tdoc->txt[tdoc->h->sumu] = post->id;
				tdoc->tf[tdoc->h->sumu]  = post->tf;
				tdoc->h->sumu++;
				tdoc->h->sumtf += post->tf;
				if (post->tf > tdoc->h->maxtf)
					tdoc->h->maxtf = post->tf;
				tdoc->h->sumsqtf = post->tf * post->tf;
				logtf = log((double)(post->tf));
				tdoc->h->sumsqlogtf += logtf * logtf;
				if (size < (tdoc->h->sumu * 4)) {
					while ((size <<= 1) < (tdoc->h->sumu * 4))
						;
					tdoc->txt = erealloc(tdoc->txt, size);
					tdoc->tf  = erealloc(tdoc->tf,  size);
				}
			}
		}

		freehash(hp, free);

		/* skip a broken document */
		if (tdoc->h->id == 0 || tdoc->h->sumu == 0) {
			/* reset */
			e++;
			weprintf("broken doc %d: %s, id = %d, sumu = %d",
				 e, docid, tdoc->h->id, tdoc->h->sumu);
			freeTDoc(tdoc);
			continue;
		}
		
		/* complete TDocHeader */
		crc = crc_wordwise(&crc_m, 0, NULL, 0);
		crc = crc_wordwise(&crc_m, crc, (uint8_t *)tdoc->h,   BTDOCHEADER);
		crc = crc_wordwise(&crc_m, crc, (uint8_t *)tdoc->txt, tdoc->h->sumu * 4);
		crc = crc_wordwise(&crc_m, crc, (uint8_t *)tdoc->tf,  tdoc->h->sumu * 4);
		tdoc->h->crc = crc;
		
		/* Attach TDoc to TFile->list */
		tfile->list = addfront(tfile->list, newnode((void *)tdoc));

		/* update THeader */
		tfile->h->nd++;
		tfile->h->b     += BTDOCHEADER + 2 * tdoc->h->sumu * 4;
		tfile->h->sumb  += tdoc->h->sumb;
		tfile->h->sumu  += tdoc->h->sumu;
		tfile->h->sumtf += tdoc->h->sumtf;
			
		fprintf(stderr, "\rdocs parsed %d", tfile->h->nd);
	}
	
	fprintf(stderr, " skipped %d\n", e);

	tfile->h->b += BTHEADER + tfile->h->bt + tfile->h->bd;

	crc = crc_wordwise(&crc_m, 0, NULL, 0);
	crc = crc_wordwise(&crc_m, crc, (uint8_t *)tfile->h, BTHEADER);
	tfile->h->crc = crc;

	__deinitcrc();

	return tfile;
}

TFile *parseq(THeader *h, Hash *hv, Parser *parser, FILE *fp)
{
	uint32_t b, tid, did, i, l, size, stemlen, e, crc;
	uint8_t *buf, s[MAXTERMLEN+1], *bp, docid[MAXTERMLEN+1];
	TFile   *tfile;
	TDoc    *tdoc;
	Token   *t_, t__;
	Node    n__, *npt_, *npt, *nppost, *nppost_;
	Post    *post, *post_;
	double  logtf;
	Hash    *hp;

	t__.str = s;
	t__.l   = 0;
	t__.t   = 0x00;
	t__.id  = 0;

	tid  = 1;
	did  = 1;
	e    = 0;
	size = MB;
	
	__initcrc();

	tfile = newTFile();

	/* copy the global stats of the given vocabulary block and patch the
	 * new TFile pointer to point to this vocabulary */
	
	tfile->h->nt = h->nt;
	tfile->h->bt = h->bt;
	freehash(tfile->ht, freetoken);
	tfile->ht = hv;
	
	while ((b = getdoc(&buf, parser, fp)) != 0) {

		tdoc = newTDoc();
		hp   = newhash(NHASH, cmppost, hashpost);
		
		/* process the doc id: hash the docid string and
		 * assign a id */

		strcpy((char *)docid, (char *)buf);
		t_   = newtoken(docid, strlen((char *)docid), 0x00, 0);
		npt_ = newnode(t_);
		npt  = hlookup(tfile->hd, npt_, 1);
		if (npt == npt_) { /* new doc, update headers */
			((Token *)(npt->data))->id = did++;
			tdoc->h->id   = ((Token *)(npt->data))->id;
			tfile->h->bd += ((Token *)(npt->data))->l + 1 + 4;
		}
		else
			freenode(npt_, freetoken);
		
		/* process the tokens */
		
		l = 0;

		for(bp = buf+MAXTERMLEN+1, i = MAXTERMLEN+1; i < b; i++, bp++) {
			
			if (*bp == '\0') {
				
				t__.str[l] = *bp;
				t__.l = l;
				l = 0;

				/* drop a short or long token */
				if (t__.l < parser->x_min || t__.l > parser->x_max) 
					continue;
				
				 /* drop too common a token */
				if (parser->c == 1) {
					n__.data = t__.str;
					npt = hlookup(parser->hcw, &n__, 0);
					if (npt != NULL)
						continue;
				}
				
				/* normalize the token */
				if (parser->n == 1) {
					stemlen = stem(parser->stemmer, (char *)t__.str, t__.l - 1);
					t__.str[stemlen + 1] = '\0';
					t__.l = stemlen + 1;
				}

				/* compute document's byte-length */
				tdoc->h->sumb += t__.l;
			
				/* lookup the token and ignore it if
				 * it is not found */
				n__.data  = &t__;
				if ((npt = hlookup(tfile->ht, &n__, 0)) == NULL)
					continue;
				
				/* build Post, hash it to sum the tf */
				post_   = newpost(((Token *)(npt->data))->id, 1);
				nppost_ = newnode(post_);
				nppost  = hlookup(hp, nppost_, 1);
				if (nppost != nppost_) { /* repeating token */
					((Post *)(nppost->data))->tf++;
					freenode(nppost_, free);
				}

				continue;
			}
			
			t__.str[l++] = *bp;
		}

		/* empty the Posts hash 'hp' into TDoc's payload and
		 * header */
			
		for (int j = 0; j < hp->n; j++) {
			for (Node *np = hp->tab[j]; np != NULL; np = np->next) {
				post = (Post *)(np->data);
				tdoc->txt[tdoc->h->sumu] = post->id;
				tdoc->tf[tdoc->h->sumu]  = post->tf;
				tdoc->h->sumu++;
				tdoc->h->sumtf += post->tf;
				if (post->tf > tdoc->h->maxtf)
					tdoc->h->maxtf = post->tf;
				tdoc->h->sumsqtf = post->tf * post->tf;
				logtf = log((double)(post->tf));
				tdoc->h->sumsqlogtf += logtf * logtf;
				if (size < (tdoc->h->sumu * 4)) {
					while ((size <<= 1) < (tdoc->h->sumu * 4))
						;
					tdoc->txt = erealloc(tdoc->txt, size);
					tdoc->tf  = erealloc(tdoc->tf,  size);
				}
			}
		}

		freehash(hp, free);

		/* skip a broken document */
		if (tdoc->h->id == 0 || tdoc->h->sumu == 0) {
			/* reset */
			e++;
			weprintf("broken doc %d: %s, id = %d, sumu = %d",
				 e, docid, tdoc->h->id, tdoc->h->sumu);
			freeTDoc(tdoc);
			continue;
		}
		
		/* complete TDocHeader */
		crc = crc_wordwise(&crc_m, 0, NULL, 0);
		crc = crc_wordwise(&crc_m, crc, (uint8_t *)tdoc->h,   BTDOCHEADER);
		crc = crc_wordwise(&crc_m, crc, (uint8_t *)tdoc->txt, tdoc->h->sumu * 4);
		crc = crc_wordwise(&crc_m, crc, (uint8_t *)tdoc->tf,  tdoc->h->sumu * 4);
		tdoc->h->crc = crc;
		
		/* Attach TDoc to TFile->list */
		tfile->list = addfront(tfile->list, newnode((void *)tdoc));

		/* update THeader */
		tfile->h->nd++;
		tfile->h->b     += BTDOCHEADER + 2 * tdoc->h->sumu * 4;
		tfile->h->sumb  += tdoc->h->sumb;
		tfile->h->sumu  += tdoc->h->sumu;
		tfile->h->sumtf += tdoc->h->sumtf;
			
		fprintf(stderr, "\rdocs parsed %d", tfile->h->nd);
	}
	
	fprintf(stderr, " skipped %d\n", e);

	tfile->h->b += BTHEADER + tfile->h->bt + tfile->h->bd;

	crc = crc_wordwise(&crc_m, 0, NULL, 0);
	crc = crc_wordwise(&crc_m, crc, (uint8_t *)tfile->h, BTHEADER);
	tfile->h->crc = crc;

	__deinitcrc();

	return tfile;
}
