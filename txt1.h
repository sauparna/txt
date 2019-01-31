#define KB 1024
#define MB 1048576
#define ASCII 128
#define CR 13
#define LF 10
#define BUFSIZE 10240
#define MINTERMLEN 1
#define MAXTERMLEN 59 /* Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch */
#define NHASH  4093
#define NHASHT 100003
#define NHASHD 100003

/* typedef struct Doc Doc; */
typedef struct Token Token;
typedef struct Term Term;
typedef struct Post Post;
typedef struct Hit Hit;
typedef struct Model Model;

typedef	double (tf_fn)(uint32_t, uint32_t, uint32_t, uint32_t,
		       uint32_t, uint32_t, double,   
		       uint32_t, uint32_t, uint32_t, uint32_t);
		       
typedef	double (df_fn)(uint32_t, uint32_t);

enum {KEEPCASE, LOWERCASE};

typedef enum {CTRLCHAR = 1, PRINTCHAR, SEPCHAR} E_asciitype;

struct Token {
	uint8_t  *str;
	uint32_t l;
	uint32_t id;
	uint8_t  t;
};

struct Post {
	uint32_t id;
	uint32_t tf;
};

struct Term {
	uint32_t id;
	uint32_t df;
	Node *plist;
};

struct Hit {
	uint32_t id;
	double n;
};

struct Model {
	tf_fn *tf;
	df_fn *df;
	tf_fn *qtf;
	df_fn *qdf;	
};

/* TODO: move to libk */
void walkhash(Hash*, void (*f)(void*, void*), void*);
uint32_t _inthash(void*, uint32_t);
int cmp_p(void*, void*);

int      cmpdoc_id(void*, void*);

/* Token */
Token     *newtoken(uint8_t*, uint32_t, uint8_t, uint32_t);
void      freetoken(void*);
int       cmptoken_str(void*, void*);
int       cmptoken_id(void*, void*);
uint32_t  hashtoken_str(void*, uint32_t);
uint32_t  hashtoken_id(void*, uint32_t);

/* Post */
Post     *newpost(uint32_t, uint32_t);
uint32_t hashpost(void*, uint32_t);
int      cmppost(void*, void*);
int      cmppost_tf(const void*, const void*);
void     fprintpost(void*, FILE*);

/* Term */
Term     *newterm(uint32_t, uint32_t, Post*);
void     freeterm(void*);
uint32_t hashterm(void*, uint32_t);
int      cmpterm(void*, void*);
int      term_match_handler(void*, void*);
void     fprintterm(void*, FILE*);

/* Hit */
Hit      *newhit(uint32_t, double);
uint32_t hashhit(void*, uint32_t);
int      cmphit(void*, void*);
void     fprinthit(void*, FILE*);

/* Models */

/** SMART **/

tf_fn SMART_n_n_tf;
tf_fn SMART_b_n_tf;
tf_fn SMART_d_b_tf;
tf_fn SMART_d_n_tf;

df_fn SMART__n__df;
df_fn SMART__t__df;

/** OKAPI **/

tf_fn OKAPI_BM25_tf;
df_fn OKAPI_BM25_df;
tf_fn OKAPI_BM25_qtf;
