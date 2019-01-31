#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <kak/eprintf.h>
#include <kak/kcommon.h>
#include <kak/klist.h>
#include <kak/khash.h>
#include "tfile.h"
#include "txt.h"

THeader *_newTHeader(void)
{
	THeader *h;
	h = (THeader *)emalloc(sizeof(THeader));
	h->type  = 0x01;
	h->bo    = THEADER;
	h->b     = 0;
	h->nt    = 0;
	h->nd    = 0;
	h->bt    = 0;
	h->bd    = 0;
	h->sumb  = 0;
	h->sumu  = 0;
	h->sumtf = 0;
	return h;
}

TDocHeader *_newTDocHeader(void)
{
	TDocHeader *h;
	h = (TDocHeader *)emalloc(sizeof(TDocHeader));
	h->type    = 0x02;
	h->bo      = TDOCHEADER;
	h->id      = 0;
	h->sumb    = 0;
	h->sumu    = 0;
	h->sumtf   = 0;
	h->maxtf   = 0;
	h->sumsqtf = 0;
	h->sumsqlogtf = 0.0;
	return h;
}

int cmpdoc(void *d1, void *d2)
{
	return ((TDocHeader *)d1)->id - ((TDocHeader *)d2)->id;
}

uint32_t hashdoc(void *data, uint32_t hsize)
{
	return _inthash(&(((TDocHeader *)data)->id), hsize);
}

TDoc *newTDoc(void)
{
	TDoc *tdoc;
	tdoc = (TDoc *)emalloc(sizeof(TDoc));
	tdoc->h   = _newTDocHeader();
	tdoc->txt = (uint32_t *)emalloc(sizeof(uint32_t) * NTERM);
	tdoc->tf  = (uint32_t *)emalloc(sizeof(uint32_t) * NTERM);
	return tdoc;
}

void freeTDoc(void *d)
{
	if (d == NULL)
		return;
	TDoc *tdoc;
	tdoc = (TDoc *)d;
	free(tdoc->tf);
	free(tdoc->txt);
	free(tdoc->h);
	free(tdoc);
}

TFile *newTFile(void)
{
	TFile *t;
	t = (TFile *)emalloc(sizeof(TFile));
	t->h    = _newTHeader();
	t->ht   = newhash(NHASHT, cmptoken_str, hashtoken_str);
	t->hd   = newhash(NHASHD, cmptoken_str, hashtoken_str);
	t->hdh  = NULL;
	t->list = NULL;
	return t;
}

void freeTFile(void *d)
{
	if (d == NULL)
		return;
	TFile *t;
	Node  *np, *next;
	t = (TFile *)d;

	np = t->list;
	for (; np != NULL; np = next) {
		next = np->next;
		freeTDoc(np->data);
		free(np);
	}

	freehash(t->hd,  freetoken);
	freehash(t->ht,  freetoken);
	freehash(t->hdh, free);
	free(t->h);
	free(t);
}

void _printTHeader(THeader *h, FILE *stream)
{
	fprintf(stream,
		"%-2s %-2s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n",
		"T",    "BO",   "B",
		"NT",   "BT",   "ND",  "BD",
		"SUMB", "SUMU", "SUMTF");
	fprintf(stream,
		"%-2u %-2u %-10u %-10u %-10u %-10u %-10u %-10u %-10u %-10u\n",
		h->type, h->bo,   h->b,
		h->nt,   h->bt,   h->nd,  h->bd,
		h->sumb, h->sumu, h->sumtf);
}

void _printTDocHeader(TDocHeader *h, FILE *stream)
{
	fprintf(stream,
		"%-2s %-2s %-10s %-10s %-10s %-10s %-10s %-10s %-16s\n",
		"T",       "BO",   "ID",
		"SUMB",    "SUMU", "SUMTF", "MAXTF",
		"SUMSQTF", "SUMSQLOGTF");
	fprintf(stream,
		"%-2u %-2u %-10u %-10u %-10u %-10u %-10u %-10u %+1.8le\n",
		h->type,    h->bo,   h->id,
		h->sumb,    h->sumu, h->sumtf, h->maxtf,
		h->sumsqtf, h->sumsqlogtf);
}

void _printTDoc(TDoc *tdoc, FILE *stream)
{
	char tid[11], tf[11]; /* max 10 digits uint32_t */
	_printTDocHeader(tdoc->h, stream);
	fprintf(stream, "ID %u\n", tdoc->h->id);
	for (int i = 0; i < tdoc->h->sumu; i++) {
		sprintf(tid, "%u", tdoc->txt[i]);
		sprintf(tf,  "%u", tdoc->tf[i]);
		fprintf(stream, "%s:%s ", tid, tf);
	}
	fprintf(stream, "\n");
}

void printTFile(TFile *tfile, FILE *stream)
{
	Node *p;
	TDoc *tdoc;
	
	_printTHeader(tfile->h, stream);
	for (p = tfile->list; p != NULL; p = p->next) {
		tdoc = p->data;
		_printTDoc(tdoc, stream);
	}
}

/* TODO: check if fmt length and parameter count matches */

int _pack(uint8_t *buf, char *fmt, ...)
{
	va_list  args;
	char     *p;
	uint8_t  *bp, *pc;
	uint16_t *ps, s;
	uint32_t *pi, i;
	uint64_t *pl, l;
	double   *pd, d;
	char     fstr[BFLOAT+1];
	int      n;

	bp = buf;
	va_start(args, fmt);
	for (p = fmt; *p != '\0'; p++) {
		switch (*p) {
		case 'c': /* char or uint8_t */
			*bp++ = va_arg(args, int);
			break;
		case 's': /* short or uint16_t */
			s = va_arg(args, int);
			*bp++ = s >> 8;
			*bp++ = s;
			break;
		case 'i': /* int or uint32_t */
			i = va_arg(args, uint32_t);
			*bp++ = i >> 24;
			*bp++ = i >> 16;
			*bp++ = i >> 8;
			*bp++ = i;
			break;
		case 'l': /* long or uint64_t */
			l = va_arg(args, uint64_t);
			*bp++ = l >> 56;
			*bp++ = l >> 48;
			*bp++ = l >> 40;
			*bp++ = l >> 32;
			*bp++ = l >> 24;
			*bp++ = l >> 16;
			*bp++ = l >> 8;
			*bp++ = l;
			break;
		case 'f': /* float */
			d = va_arg(args, double);
			sprintf(fstr, "%+1.8le", d);
			memcpy(bp, fstr, BFLOAT);
			bp += BFLOAT;
			break;
		case 'C':
			pc = va_arg(args, uint8_t *);
			n = va_arg(args, int);
			for (int j = 0; j < n; j++)
				*bp++ = *(pc+j);
			break;
		case 'S':
			ps = va_arg(args, uint16_t *);
			n = va_arg(args, int);
			for (int j = 0; j < n; j++) {
				*bp++ = *(ps+j) >> 8;
				*bp++ = *(ps+j);
			}
			break;
		case 'I':
			pi = va_arg(args, uint32_t *);
			n = va_arg(args, int);
			for (int j = 0; j < n; j++) {
				*bp++ = *(pi+j) >> 24;
				*bp++ = *(pi+j) >> 16;
				*bp++ = *(pi+j) >> 8;
				*bp++ = *(pi+j);
			}
			break;
		case 'L':
			pl = va_arg(args, uint64_t *);
			n = va_arg(args, int);
			for (int j = 0; j < n; j++) {
				*bp++ = *(pl+j) >> 56;
				*bp++ = *(pl+j) >> 48;
				*bp++ = *(pl+j) >> 40;
				*bp++ = *(pl+j) >> 32;
				*bp++ = *(pl+j) >> 24;
				*bp++ = *(pl+j) >> 16;
				*bp++ = *(pl+j) >> 8;
				*bp++ = *(pl+j);
			}
			break;
		case 'F':
			pd = va_arg(args, double *);
			n = va_arg(args, int);
			for (int j = 0; j < n; j++) {
				sprintf(fstr, "%+1.8le", *(pd+j));
				memcpy(bp, fstr, BFLOAT);
				bp += BFLOAT;
			}
			break;
		default:
			va_end(args);
			return -1;
		}
	}
	va_end(args);
	return bp - buf;
}

/* TODO: check if fmt length and parameter count matches */

int _unpack(uint8_t *buf, char *fmt, ...)
{
	va_list  args;
	char     *p;
	uint8_t  *bp, *pc;
	uint16_t *ps;
	uint32_t *pi;
	uint64_t *pl;
	double   *pd;
	char     fstr[BFLOAT+1];
	int      n;

	fstr[BFLOAT] = '\0'; /* BFLOAT = 17, say. Here is a safe guard
			      * placed in the 17th. byte; in case the
			      * floating point string produced during
			      * packing was 16 bytes long. If it was,
			      * then the 16 bytes being unpacked from
			      * the buffer will not have a trailing
			      * NULL terminator. Otherwise, if it was
			      * a 15-byte string, then it will have
			      * had the NULL terminator in the
			      * 16th. byte, because the string
			      * representation was produced by a
			      * sprint(buf, '+1.8e', d) which appends
			      * one at the end. */
	
	bp = buf;
	va_start(args, fmt);
	for (p = fmt; *p != '\0'; p++) {
		switch (*p) {
		case 'c': /* char or uint8_t */
			pc = va_arg(args, uint8_t *);
			*pc = *bp++;
			break;
		case 's': /* short or uint16_t */
			ps = va_arg(args, uint16_t *);
			*ps  = (uint16_t)*bp++ << 8;
			*ps |= (uint16_t)*bp++;
			break;
		case 'i': /* int or uint32_t */
			pi = va_arg(args, uint32_t *);
			*pi  = (uint32_t)*bp++ << 24;
			*pi |= (uint32_t)*bp++ << 16;
			*pi |= (uint32_t)*bp++ << 8;
			*pi |= (uint32_t)*bp++;
			break;
		case 'l': /* long or uint64_t */
			pl = va_arg(args, uint64_t *);
			*pl  = (uint64_t)*bp++ << 56;
			*pl |= (uint64_t)*bp++ << 48;
			*pl |= (uint64_t)*bp++ << 40;
			*pl |= (uint64_t)*bp++ << 32;
			*pl |= (uint64_t)*bp++ << 24;
			*pl |= (uint64_t)*bp++ << 16;
			*pl |= (uint64_t)*bp++ << 8;
			*pl |= (uint64_t)*bp++;
			break;
		case 'f': /* float */
			pd = va_arg(args, double *);
			memcpy(fstr, bp, BFLOAT);
			bp += BFLOAT;
			sscanf(fstr, "%le", pd);
			break;
		case 'C': /* char or uint8_t */
			pc = va_arg(args, uint8_t *);
			n  = va_arg(args, int);
			for (int j = 0; j < n; j++)
				*(pc+j) = *bp++;
			break;
		case 'S': /* short or uint16_t */
			ps = va_arg(args, uint16_t *);
			n  = va_arg(args, int);
			for (int j = 0; j < n; j++) {
				*(ps+j)  = (uint16_t)*bp++ << 8;
				*(ps+j) |= (uint16_t)*bp++;
			}
			break;
		case 'I': /* int or uint32_t */
			pi = va_arg(args, uint32_t *);
			n  = va_arg(args, int);
			for (int j = 0; j < n; j++) {
				*(pi+j)  = (uint32_t)*bp++ << 24;
				*(pi+j) |= (uint32_t)*bp++ << 16;
				*(pi+j) |= (uint32_t)*bp++ << 8;
				*(pi+j) |= (uint32_t)*bp++;
			}
			break;
		case 'L': /* long or uint64_t */
			pl = va_arg(args, uint64_t *);
			n  = va_arg(args, int);
			for (int j = 0; j < n; j++) {
				*(pl+j)  = (uint64_t)*bp++ << 56;
				*(pl+j) |= (uint64_t)*bp++ << 48;
				*(pl+j) |= (uint64_t)*bp++ << 40;
				*(pl+j) |= (uint64_t)*bp++ << 32;
				*(pl+j) |= (uint64_t)*bp++ << 24;
				*(pl+j) |= (uint64_t)*bp++ << 16;
				*(pl+j) |= (uint64_t)*bp++ << 8;
				*(pl+j) |= (uint64_t)*bp++;
			}
			break;
		case 'F': /* float */
			pd = va_arg(args, double *);
			n  = va_arg(args, int);
			for (int j = 0; j < n; j++) {
				memcpy(fstr, bp, BFLOAT);
				bp += BFLOAT;
				sscanf(fstr, "%le", pd+j);
			}
			break;
		default:
			va_end(args);
			return -1;
		}
	}
	va_end(args);
	return bp - buf;
}

int packTHeader(uchar *buf, THeader *h)
{
	return _pack(buf, "cciiiiiiii",
		     h->type,  h->bo,   h->b,
		     h->nt,    h->nd,   h->bt,  h->bd,
		     h->sumb,  h->sumu, h->sumtf);
}

int pack_idmap(uint8_t *buf, Hash *h)
{
	/* A n-byte string, a NULL terminator '\0', and a 4-byte int
	 * is packed into n+1+4 bytes. */
	
	uint8_t *bp;
	Token *token;
	bp = buf;
	for (int i = 0; i < h->n; i++) {
		for (Node *np = h->tab[i]; np != NULL; np = np->next) {
			token = (Token *)(np->data);
			memcpy(bp, token->str, token->l + 1); /* include '\0' */
			bp += token->l + 1;
			*bp++ = token->id >> 24;
			*bp++ = token->id >> 16;
			*bp++ = token->id >> 8;
			*bp++ = token->id;
		}
	}
	return bp - buf;
}

int packTDocHeader(uchar *buf, TDocHeader *h)
{
	return _pack(buf, "cciiiiiif",
		     h->type,    h->bo,   h->id,
		     h->sumb,    h->sumu, h->sumtf, h->maxtf,
		     h->sumsqtf, h->sumsqlogtf);
}

int packTDoc_payload(uchar *buf, TDoc *tdoc)
{
	return _pack(buf, "II",
		     tdoc->txt, tdoc->h->sumu,
		     tdoc->tf,  tdoc->h->sumu);
}

int unpackTHeader(THeader *h, int n, uint8_t *buf)
{
	int m;
	if ((m = _unpack(buf, "cciiiiiiii",
			 &h->type, &h->bo,   &h->b,
			 &h->nt,   &h->nd,   &h->bt,   &h->bd,
			 &h->sumb, &h->sumu, &h->sumtf)) != n)
		return -1;
	assert(h->type == 0x01);
	return m;
}

int unpack_idmap(Hash *h, int n, uint8_t *buf)
{
	/* A n-byte string, a NULL terminator '\0', and a 4-byte int
	 * is packed into n+1+4 bytes. */

	uint8_t *bp;
	uint8_t s[MAXTERMLEN + 1];
	uint32_t l;
	int m;
	Token token__, *token_;
	Node *nptoken_;

	token__.str = s;
	token__.l   = 0;
	token__.id  = 0;
	token__.t   = 0;
	bp = buf;
	l  = 0;
	m  = 0;
	for(int i = 0; i < n; i++) {
		if (*(bp+i) == '\0') {
			/* unpack from buffer into local memory */
			token__.str[l] = *(bp + i++);
			token__.l = l;
			token__.id  = (uint32_t)*(bp + i++) << 24;
			token__.id |= (uint32_t)*(bp + i++) << 16;
			token__.id |= (uint32_t)*(bp + i++) << 8;
			token__.id |= (uint32_t)*(bp + i);
			m += l + 1 + 4;
			l = 0;
			/* build Token and insert it into hash table */
			token_   = newtoken(token__.str, token__.l, token__.t, token__.id);
			nptoken_ = newnode(token_);
			hlookup(h, nptoken_, 1);
			continue; /* FIXIT: continue even if i has been incremented? */
		}
		token__.str[l++] = *(bp+i);
	}
	if (m != n)
		return -1;
	return m;
}

int unpackTDocHeader(TDocHeader *h, int n, uint8_t *buf)
{
	int m;
	if ((m = _unpack(buf, "cciiiiiif",
			 &h->type,    &h->bo,   &h->id,
			 &h->sumb,    &h->sumu, &h->sumtf, &h->maxtf,
			 &h->sumsqtf, &h->sumsqlogtf)) != n)
		return -1;
	assert(h->type == 0x02);
	return m;
}

int unpackTDoc_payload(TDoc *tdoc, int n, uint8_t *buf)
{
	int m;
	if ((m = _unpack(buf, "II",
			 tdoc->txt, tdoc->h->sumu,
			 tdoc->tf,  tdoc->h->sumu)) != n)
		return -1;
	return m;
}

void writeTFile(FILE *fp, TFile *tfile)
{
	int n, c, size, bpack;
	Node  *np;
	TDoc  *tdoc;
	uchar *buf;

	size = MB;
	buf  = (uchar *)emalloc(size);
	
	c = 0;

	/* THeader */
	if ((n = packTHeader(buf, tfile->h)) == -1)
		eprintf("packTheader() failed on THeader\n");
	if (fwrite(buf, n, 1, fp) != 1)
		eprintf("fwrite() failed on %d byte THeader\n", n);

	/* tfile->ht and tfile->hd */
	bpack = tfile->h->bt > tfile->h->bd ? tfile->h->bt : tfile->h->bd;
	if (size < bpack) {
		while ((size <<= 1) < bpack);
		buf = erealloc(buf, size);
	}
	if ((n = pack_idmap(buf, tfile->ht)) == -1)
		eprintf("pack_idmap() failed on tfile->ht\n");
	if (fwrite(buf, n, 1, fp) != 1)
		eprintf("fwrite() failed on %d byte, packed, tfile->ht\n", n);
	if ((n = pack_idmap(buf, tfile->hd)) == -1)
		eprintf("pack_idmap() failed on tfile->hd\n");
	if (fwrite(buf, n, 1, fp) != 1)
		eprintf("fwrite() failed on %d byte, packed, tfile->hd\n", n);
	
	/* list of TDocs */
	for (np = tfile->list; np != NULL; np = np->next) {

		tdoc = np->data;

		/* TDocHeader */
		if ((n = packTDocHeader(buf, tdoc->h)) == -1)
			eprintf("packTDocHeader() failed\n");
		if (fwrite(buf, n, 1, fp) != 1)
			eprintf("fwrite() failed on %d byte TDocHeader\n", n);

		/* TDoc */
		bpack = 2 * tdoc->h->sumu * 4;
		if (size < bpack) {
			while ((size <<= 1) < bpack);
			buf = erealloc(buf, size);
		}
		if ((n = packTDoc_payload(buf, tdoc)) == -1)
			eprintf("packTDoc_payload() failed on %d byte TDoc payload\n", bpack);
		if (fwrite(buf, n, 1, fp) != 1)
			eprintf("fwrite() failed on %d byte TDoc payload\n", n);

		c++;
		fprintf(stderr, "\rTDocs sent: %d/%u", c, tfile->h->nd);
	}
	
	free(buf);
	if (c < tfile->h->nd)
		fprintf(stderr, "; TDocs skipped: %d", tfile->h->nd - c);
	fprintf(stderr, "\n");
}

TFile *readTFile(FILE *fp, TFile *tfile)
{
	int n, c, size, bpack;
	/* TFile *tfile; */
	TDoc *tdoc;
	uchar *buf;

	/* tfile = newTFile(); */
	
	size = MB;
	buf  = (uchar *)emalloc(size);

	c = 0;
	
	if ((n = fread(buf, BTHEADER, 1, fp)) != 1)
		eprintf("fread() failed on %d byte THeader\n",
			BTHEADER);
	if (unpackTHeader(tfile->h, BTHEADER, buf) != BTHEADER)
		eprintf("unpackTHeader() failed on %d byte THeader\n",
			BTHEADER);

	bpack = tfile->h->bt > tfile->h->bd ? tfile->h->bt : tfile->h->bd;
	if (size < bpack) {
		while ((size <<= 1) < bpack);
		buf = erealloc(buf, size);
	}

	if ((n = fread(buf, tfile->h->bt, 1, fp)) != 1)
		eprintf("fread() failed on %d byte tfile->h->bt\n",
		       tfile->h->bt);
	if (unpack_idmap(tfile->ht, tfile->h->bt, buf) != tfile->h->bt)
		eprintf("unpack_idmap() failed on %d byte tfile->h->bt\n",
			tfile->h->bt);
	if ((n = fread(buf, tfile->h->bd, 1, fp)) != 1)
		eprintf("fread() failed on %d byte tfile->h->bd\n",
		       tfile->h->bd);
	if (unpack_idmap(tfile->hd, tfile->h->bd, buf) != tfile->h->bd)
		eprintf("unpack_idmap() failed on %d byte tfile->h->bd\n",
			tfile->h->bd);
	
	for (int i = 0; i < tfile->h->nd; i++) {

		tdoc = newTDoc();

		if ((n = fread(buf, BTDOCHEADER, 1, fp)) != 1)
			eprintf("fread() failed on %d bytes TDocHeader for doc %d of %d\n",
				BTDOCHEADER, i, tfile->h->nd);
		if ((n = unpackTDocHeader(tdoc->h, BTDOCHEADER, buf)) != BTDOCHEADER)
			eprintf("unpackTDocHeader() failed on %d byte TDocHeader for doc %d of %d\n",
				BTDOCHEADER, i, tfile->h->nd);

		bpack = 2 * tdoc->h->sumu * 4;
		
		if (size < bpack) {
			while ((size <<= 1) < bpack);
			buf = erealloc(buf, size);
		}
		
		if ((n = fread(buf, bpack, 1, fp)) != 1)
			eprintf("fread() failed on %d byte payload\n",
				bpack);
		if ((n = unpackTDoc_payload(tdoc, bpack, buf)) != bpack)
			eprintf("unpackTDoc_payload() failed on %d byte payload\n",
				bpack);
		
		/* add payload to list */
		tfile->list = addfront(tfile->list, newnode((void *)tdoc));

		c++;
		fprintf(stderr, "\rTDocs read: %d/%d", i+1, tfile->h->nd);
	}
	
	free(buf);
	if (c < tfile->h->nd)
		fprintf(stderr, "; TDocs skipped: %d", tfile->h->nd - c);
	fprintf(stderr, "\n");

	return tfile;
}
