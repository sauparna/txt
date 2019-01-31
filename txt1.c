#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <kak/eprintf.h>
#include <kak/kcommon.h>
#include <kak/klist.h>
#include <kak/khash.h>
#include "tfile.h"
#include "txt.h"

/* TODO: move to libk */
void walkhash(Hash *h, void (*process_data)(void*, void*), void *arg)
{	
	for (int i = 0; i < h->n; i++) {
		for (Node *np = h->tab[i]; np != NULL; np = np->next) {
			(*process_data)(np->data, arg);
		}
	}
}

/* TODO: move to libk */
uint32_t _inthash(void *n, uint32_t hsize) {return *((uint32_t *)n) % hsize;}

int cmp_p(void *d1, void *d2) {return d1 - d2;}

/* Doc */

int cmpdoc_id(void *d1, void *d2)
{
	/* TODO: (TDocHeader *)d1)->id && ((TDocHeader *)d2)->id ? */
	return ((TDocHeader *)d1)->id - ((TDocHeader *)d2)->id;
}

/* Token */

Token *newtoken(uint8_t *str, uint32_t l, uint8_t type, uint32_t id)
{
	Token *t;
	t = (Token *)emalloc(sizeof(Token));
	t->str = estrdup(str);
	t->l   = l;
	t->t   = type;
	t->id  = id;
	return t;
}

void freetoken(void *d)
{
	if (d == NULL)
		return;
	free(((Token*)d)->str);
	free(d);
}

int cmptoken_str(void *d1, void *d2)
{	
	return strcmp(((Token *)d1)->str, ((Token *)d2)->str);
}

int cmptoken_id(void *d1, void *d2)
{	
	return ((Token *)d1)->id - ((Token *)d2)->id;
}

uint32_t  hashtoken_str(void *data, uint32_t hsize)
{
	return _strhash(((Token *)data)->str, hsize);
}

uint32_t  hashtoken_id(void* data, uint32_t hsize)
{
	return _inthash(&(((Token *)data)->id), hsize);
}

/* Post */

Post *newpost(uint32_t id, uint32_t tf)
{
	Post *p;
	p = (Post *)emalloc(sizeof(Post));
	p->id = id;
	p->tf = tf;
	return p;
}

int cmppost(void *d1, void *d2)
{
	return ((Post *)d1)->id - ((Post *)d2)->id;
}

int cmppost_tf(const void *d1, const void *d2)
{
	return ((Post *)d1)->tf - ((Post *)d2)->tf;
}

uint32_t hashpost(void *data, uint32_t hsize)
{
	return _inthash(&(((Post *)data)->id), hsize);
}

void fprintpost(void *data, FILE *stream)
{
	Post *p;
	p = (Post *)data;
	fprintf(stream, " %u:%u", p->id, p->tf);
}

/* Term */

Term *newterm(uint32_t id, uint32_t df, Post *post)
{
	Term *t;
	t = (Term *)emalloc(sizeof(Term));
	t->id = id;
	t->df = df;
	t->plist = newnode(post);
	return t;
}

void freeterm(void *d)
{
	if (d == NULL)
		return;
	Term *t;
	Node *np, *next;
	t = (Term *)d;
	
	np = t->plist;
	for (; np != NULL; np = next) {
		next = np->next;
		free(np->data);
		free(np);
	}
	free(t);
}

uint32_t hashterm(void *data, uint32_t hsize)
{
	return _inthash(&(((Term *)data)->id), hsize);
}

int cmpterm(void *d1, void *d2)
{
	return ((Term *)d1)->id - ((Term *)d2)->id;
}

int term_match_handler(void *d1, void *d2)
{
	Term *t, *newt;
	Post *p, *newp;

	newt = (Term *)d1;
	t = (Term *)d2;

	newp = (Post *)(newt->plist->data);
	p = (Post *)(t->plist->data);

	if (cmppost(newp, p) == 0)
		p->tf++;
	else {
		t->plist = addfront(t->plist, newt->plist);
		t->df++;
	}
	
	return 0;
}

void fprintterm(void *data, FILE *stream)
{
	Term *t;
	t = (Term *)data;
	fprintf(stream, " %u:%u", t->id, t->df);
	for (Node *np = t->plist; np != NULL; np = np->next)
		fprintpost(stream, np->data);
}

/* Hit */

Hit *newhit(uint32_t id, double n)
{
	Hit *h;
	h = (Hit *)emalloc(sizeof(Hit));
	h->id = id;
	h->n  = n;
	return h;
}

uint32_t hashhit(void *data, uint32_t hsize)
{
	return _inthash(&(((Hit *)data)->id), hsize);
}

int cmphit(void *d1, void *d2)
{
	return ((Hit *)d1)->id - ((Hit *)d2)->id;
}

void fprinthit(void *data, FILE *stream)
{
	Hit *h;
	h = (Hit *)data;
	fprintf(stream, " %u:%+1.8e", h->id, h->n);
}

/* models */

/** SMART **/

double SMART_n_n_tf(uint32_t tf,    uint32_t sumb,    uint32_t sumu,       uint32_t sumtf,
		    uint32_t maxtf, uint32_t sumsqtf, double   sumsqlogtf,
		    uint32_t sumb_, uint32_t sumu_,   uint32_t sumtf_,     uint32_t nd)
{
	return (double)tf;
}

double SMART_b_n_tf(uint32_t tf,    uint32_t sumb,    uint32_t sumu,       uint32_t sumtf,
		    uint32_t maxtf, uint32_t sumsqtf, double   sumsqlogtf,
		    uint32_t sumb_, uint32_t sumu_,   uint32_t sumtf_,     uint32_t nd)

{
	if (tf > 0)
		return 1.0;
}

double SMART_d_b_tf(uint32_t tf,    uint32_t sumb,    uint32_t sumu,       uint32_t sumtf,
		    uint32_t maxtf, uint32_t sumsqtf, double   sumsqlogtf,
		    uint32_t sumb_, uint32_t sumu_,   uint32_t sumtf_,     uint32_t nd)
{
	/*
	  s   = 0.2
	  tf_ = (1 + log(1 + log(tf))) / ((1 - s) * avg(dl) + s * dl)
	*/

	double tf_;
	tf_ = (1.0 + log(1.0 + log((double)tf))) / (0.8 * ((double)sumb_ / (double)nd) + 0.2 * (double)sumb);
	return tf_;
}

double SMART_d_n_tf(uint32_t tf,    uint32_t sumb,    uint32_t sumu,       uint32_t sumtf,
		    uint32_t maxtf, uint32_t sumsqtf, double   sumsqlogtf,
		    uint32_t sumb_, uint32_t sumu_,   uint32_t sumtf_,     uint32_t nd)
{
	/* 1 + log(1 + log(tf)) */
	return 1.0 + log(1.0 + log((double)tf));
}

double SMART__n__df(uint32_t df, uint32_t nd)
{
	return 1.0;
}

double SMART__t__df(uint32_t df, uint32_t nd)
{
	/* log((n + 1) / df) */
	return log(((double)nd + 1.0) / (double)df);
}

/** OKAPI **/

double OKAPI_BM25_tf(uint32_t tf,    uint32_t sumb,    uint32_t sumu,       uint32_t sumtf,
		     uint32_t maxtf, uint32_t sumsqtf, double   sumsqlogtf,
		     uint32_t sumb_, uint32_t sumu_,   uint32_t sumtf_,     uint32_t nd)
{
	/*
	  K1  = 2
	  b   = 0.8
	  tf_ = ((K1 + 1) * tf) / (K1 * (1 - b + b * (dl / avg(dl))) + tf);
	*/

	double tf_;
	tf_ = 3.0 * (double)tf / (0.4 + 1.6 * (double)sumb * (double)nd / (double)sumb_ + (double)tf);
	return tf_;
}

double OKAPI_BM25_df(uint32_t df, uint32_t nd)
{
	/* df_ = log((n - df + 0.5) / (df + 0.5)) */
	return log(((double)nd - (double)df + 0.5) / ((double)df + 0.5));
}

double OKAPI_BM25_qtf(uint32_t tf,    uint32_t sumb,    uint32_t sumu,       uint32_t sumtf,
		      uint32_t maxtf, uint32_t sumsqtf, double   sumsqlogtf,
		      uint32_t sumb_, uint32_t sumu_,   uint32_t sumtf_,     uint32_t nd)
{
	/* K3   = 1000 */
	/* qtf_ = ((K3 + 1) * qtf) / (K3 + qtf) */
	return 1001.0 * (double)tf / (1000.0 + (double)tf);
}
