#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kak/kcommon.h>
#include <kak/eprintf.h>
#include <kak/klist.h>
#include <kak/khash.h>
#include "txt.h"
#include "tfile.h"

Hash *buildii(TFile **tfc, FILE *fp)
{
	/* tfc will end up with the corpus' header, term-map, doc-map
	   and all the TDocHeaders in these place-holders,
	   respectively;
	   
	   TFile->h
	   TFile->ht
	   TFile->hd
	   TFile->hdh
	   
	   TFile->list stays empty, as TDocs will have been splintered
	   and shaped into the inverted index.
	*/
	
	int     n, size, bpack, pflag_, tflag_;
	uint8_t *buf;
	TDoc    *tdoc;
	Hash    *ii;
	Term    *term_, *term;
	Post    *post_, *post;
	Node    *npt_, *npt, *npd_;

	*tfc             = newTFile();
	(*tfc)->hdh      = newhash(NHASHD, cmpdoc, hashdoc); /* hash of TDocHeaders */

	/* dismantle the default hash table functions.*/
	(*tfc)->ht->cmp  = cmptoken_id;
	(*tfc)->ht->hash = hashtoken_id;
	(*tfc)->hd->cmp  = cmptoken_id;
	(*tfc)->hd->hash = hashtoken_id;
	
	size = MB;
	ii   = newhash(NHASHT, cmpterm, hashterm);
	buf  = (uint8_t*)emalloc(size);

	/* get THeader */
	if ((n = fread(buf, BTHEADER, 1, fp)) != 1)
		eprintf("ii(): fread() failed on %d byte THeader\n",
			BTHEADER);
	if (unpackTHeader((*tfc)->h, BTHEADER, buf) != BTHEADER)
		eprintf("ii(): unpackTHeader() failed on %d byte THeader\n",
			BTHEADER);

	/* get ID maps */
	bpack = (*tfc)->h->bt > (*tfc)->h->bd ? (*tfc)->h->bt : (*tfc)->h->bd;
	if (size < bpack) {
		while ((size <<= 1) < bpack)
			;
		buf = erealloc(buf, size);
	}
	if ((n = fread(buf, (*tfc)->h->bt, 1, fp)) != 1)
		eprintf("ii(): fread() failed on %d byte tfile->h->bt\n",   (*tfc)->h->bt);
	if (unpack_idmap((*tfc)->ht, (*tfc)->h->bt, buf) != (*tfc)->h->bt)
		eprintf("ii(): unpack_idmap() failed on %d byte tfile->ht", (*tfc)->h->bt);
	if ((n = fread(buf, (*tfc)->h->bd, 1, fp)) != 1)
		eprintf("ii(): fread() failed on %d byte tfile->h->bd\n",   (*tfc)->h->bd);
	if (unpack_idmap((*tfc)->hd, (*tfc)->h->bd, buf) != (*tfc)->h->bd)
		eprintf("ii(): unpack_idmap() failed on %d byte tfile->hd", (*tfc)->h->bd);

	/* get the documents */
	for (int i = 0; i < (*tfc)->h->nd; i++) {
		tdoc = newTDoc();

		/* get TDocHeader */
		if ((n = fread(buf, BTDOCHEADER, 1, fp)) != 1)
			eprintf("ii(): fread() failed on %d byte TDocHeader\n",
				BTDOCHEADER);
		if (unpackTDocHeader(tdoc->h, BTDOCHEADER, buf) != BTDOCHEADER)
			eprintf("ii(): unpackTDocHeader() failed on %d byte TDocHeader\n",
				BTDOCHEADER);

		/* unpack the term info */
		bpack = 2 * tdoc->h->sumu * 4;
		if (size < bpack) {
			while ((size <<= 1) < bpack)
				;
			buf = erealloc(buf, size);
		}
		if ((n = fread(buf, bpack, 1, fp)) != 1)
			eprintf("ii(): fread() failed on %d byte TDoc payload\n",
				bpack);
		if (unpackTDoc_payload(tdoc, bpack, buf) != bpack)
			eprintf("ii(): unpackTDoc_payload() failed on %d byte TDoc payload\n",
				bpack);

		/* go over the terms */
		for (int j = 0; j < tdoc->h->sumu; j++) {

			/* build a payload */
			pflag_ = tflag_ = 0;
			post_  = newpost(tdoc->h->id, tdoc->tf[j]);
			term_  = newterm(tdoc->txt[j], 1, post_);
			npt_   = newnode(term_);

			/* drop it on the inverted index */
			npt = hlookup(ii, npt_, 1);
			if (npt != npt_) {
				tflag_ = 1;
				term   = (Term *)(npt->data);
				post   = (Post *)(term->plist->data);
				if (cmppost(post_, post) == 0) {
					post->tf += post_->tf;
					pflag_ = 1;
				}
				else {
					term->plist = addfront(term->plist, term_->plist);
					term->df++;
				}
				
			}
			if (pflag_)
				freenode(term_->plist, free);
			if (tflag_)
				free(term_);
		}
		
		/* place the TDocHeader in (*tfc)->hdh */
		npd_ = newnode(tdoc->h);
		hlookup((*tfc)->hdh, npd_, 1);
		/* not checking for duplicates, documents in .t files
		 * are unique */

		/* reset */
		
		/* careful not to call freeTDoc(tdoc), the the
		 * TDocHeader is persists in tfile->hdh */

		free(tdoc->txt); tdoc->txt = NULL;
		free(tdoc->tf);  tdoc->tf  = NULL;
		free(tdoc);
		fprintf(stderr, "\rindexed: %d/%d", i+1, (*tfc)->h->nd);
	}
	fprintf(stderr, "\n");
	free(buf);
	return ii;
}

Hash *search(Hash *ii, TFile *tfc, TDoc *tdq, Model m) {

	/* tfc carries the following pieces;
	   TFile->h
	   TFile->ht
	   TFile->hd
	   TFile->hdh
	*/
	
	Hash *res;
	Term t__, *term;
	Post *post;
	Node n__, *npt, *nph_, *nph, *npd;
	double tf, df, qtf, qdf, poc;
	TDocHeader d__, *d;
	Hit *hit_;

	res       = newhash(NHASHD, cmphit, hashhit);
	t__.id    = 0;
	t__.df    = 0;
	t__.plist = NULL;
	npt       = NULL;
		
	/* go over each query-term */
	for (int j = 0; j < tdq->h->sumu; j++) {
			
		/* look it up in the inverted index */
		t__.id   = tdq->txt[j];
		n__.data = &t__;
		if ((npt = hlookup(ii, &n__, 0)) == NULL)
			continue;
		term = (Term *)(npt->data);

		/* walk the term's list of docs; aka postings
		 * list */
		for (Node *npp = term->plist; npp != NULL; npp = npp->next) {
				
			post = (Post *)(npp->data);

			/* look up the doc's info */
			d__.id = post->id;
			n__.data = &d__;
			if ((npd = hlookup(tfc->hdh, &n__, 0)) == NULL) {
				weprintf("search(): indexed doc %d not in corpus\n",
					 post->id);
				continue;
			}
			d = (TDocHeader *)(npd->data);

			/* modulate the tf, df of a doc and a
			 * query */
			tf   = (*m.tf)(post->tf,  d->sumb,    d->sumu,    d->sumtf,
				       d->maxtf,  d->sumsqtf, d->sumsqlogtf,
				       tfc->h->sumb, tfc->h->sumu,  tfc->h->sumtf, tfc->h->nd);
			df   = (*m.df)(term->df, tfc->h->nd);
			qtf  = (*m.qtf)(tdq->tf[j], d->sumb,    d->sumu,    d->sumtf,
					d->maxtf,   d->sumsqtf, d->sumsqlogtf,
					tfc->h->sumb,  tfc->h->sumu,  tfc->h->sumtf, tfc->h->nd);
			qdf  = (*m.qdf)(term->df, tfc->h->nd);

			/* compute the product of components of two vectors */
			poc = tf * df * qtf * qdf;

			/* sum the product; to partially
			 * complete the vector dot product */
			hit_ = newhit(post->id, poc);
			nph_ = newnode(hit_);
			nph  = hlookup(res, nph_, 1);
			if (nph != nph_) {
				((Hit *)(nph->data))->n += poc;
				freenode(nph_, free);
			}
		}
	}
	return res;
}

void usage(char *progname)
{
	eprintf("usage: %s [-s query] [-m model] <doc.t\n", progname);
}

int main(int argc, char *argv[])
{
	int   opt_s, opt_m;
	char  qfile[KB], mstr[KB];
	TFile *tfc, *tfq;
	TDoc  *tdq;
	Hash  *ii, *res;
	FILE  *fpq, *fplog;
	Model m;
	Hit   *hit;
	Token t__;
	Node  *npqt, *npdt, n__;
	
	esetprogname(estrdup(argv[0]));
	fplog = fopen(strcat(estrdup(argv[0]), "-error.log"), "w");
	esetstream(fplog);

	tfc = NULL;
	opt_s = opt_m = 0;
	
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-s") == 0) {
			if (i + 1 <= argc - 1) {
				strcpy(qfile, argv[++i]);
				opt_s = 1;
			}
			else
				usage(argv[0]);
		}
		else if (strcmp(argv[i], "-m") == 0) {
			if (i + 1 <= argc - 1) {
				strcpy(mstr, argv[++i]);
				opt_m = 1;
			}
			else
				usage(argv[0]);
		}
	}

	/* test model nnn.nnn, default */
	m.tf  = &SMART_n_n_tf;
	m.df  = &SMART__n__df;
	m.qtf = &SMART_n_n_tf;
	m.qdf = &SMART__n__df;

	if (opt_m) {
		if (strcmp(mstr, "SMART_dnb_dtn") == 0) {
			m.tf  = &SMART_d_b_tf;
			m.df  = &SMART__n__df;
			m.qtf = &SMART_d_n_tf;
			m.qdf = &SMART__t__df;
		}
		else if (strcmp(mstr, "SMART_dtb_nnn") == 0) {
			m.tf  = &SMART_d_b_tf;
			m.df  = &SMART__t__df;
			m.qtf = &SMART_n_n_tf;
			m.qdf = &SMART__n__df;
		}
		else if (strcmp(mstr, "SMART_bnn_bnn") == 0) {
			m.tf  = &SMART_b_n_tf;
			m.df  = &SMART__n__df;
			m.qtf = &SMART_b_n_tf;
			m.qdf = &SMART__n__df;
		}
		else if (strcmp(mstr, "OKAPI_BM25") == 0) {
			m.tf  = &OKAPI_BM25_tf;
			m.df  = &OKAPI_BM25_df;
			m.qtf = &OKAPI_BM25_qtf;
			m.qdf = &SMART__n__df;
		}
	}

	ii = buildii(&tfc, stdin);

	if (!opt_s) {
		freehash(ii, free);
		freeTFile(tfc);
		return 0;
	}

	tfq = newTFile();
	tfq->hd->cmp  = cmptoken_id;
	tfq->hd->hash = hashtoken_id;
	tfq->ht->cmp  = cmptoken_id;
	tfq->ht->hash = hashtoken_id;

	fpq = fopen(qfile, "r");
	receive(fpq, tfq);
	fclose(fpq);

	/* search */
	for (Node *npq = tfq->list; npq != NULL; npq = npq->next) {
		tdq = (TDoc *)(npq->data);
		res = search(ii, tfc, tdq, m);
		for (int i = 0; i < res->n; i++) {
			for (Node *np = res->tab[i]; np != NULL; np = np->next) {
				hit = (Hit *)(np->data);
				/* fprintf(stdout, "%u %u %+1.8e\n", */
				/* 	tdq->h->id, hit->id, hit->n); */
				
				t__.id   = tdq->h->id;
				n__.data = &t__;
				npqt     = hlookup(tfq->hd, &n__, 0);
				if (npqt == NULL)
					eprintf("query %u missing in tfq->hd\n",
						tdq->h->id);
				t__.id   = hit->id;
				n__.data = &t__;
				npdt     = hlookup(tfc->hd, &n__, 0);
				if (npdt == NULL)
					eprintf("doc %u missing in tfc->hd\n",
						hit->id);
				fprintf(stdout, "%s %s %+1.8e\n",
					((Token *)(npqt->data))->str,
					((Token *)(npdt->data))->str,
					hit->n);
			}
		}
		freehash(res, free);
	}

	freehash(ii, free);
	freeTFile(tfc);
	freeTFile(tfq);

	fclose(fplog);
	return 0;
}
