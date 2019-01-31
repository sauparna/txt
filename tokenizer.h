typedef struct Stack Stack;
typedef struct Tokenizer Tokenizer;
typedef struct Parser Parser;
typedef struct stemmer Stemmer;

enum {TERM = 1, OTAG, CTAG};

struct Stack {
	uint8_t s[KB];
	int  p;
	int  n;
};

struct Tokenizer {
	char  delimiters[ASCII];
	int   asciitab[ASCII];
	Stack *mem;
	int   casechange;
};

struct Parser {
	char type;
	int c;
	int n;
	int x_min;
	int x_max;
	Hash *hcw;        /* hash of common words */
	Stemmer *stemmer;
	int n_tag;
	char doctag[64];
	char idtag[64];
	char *tag[64];
};

void reset(Stack*, int);
void push(Stack*, char*);
char pop(Stack*);
void printstack(Stack*);
Tokenizer *newtokenizer(char*, int, Stack*);
uint32_t gettoken(Token*, FILE*, Tokenizer*);
Parser *newparser(char, char**, int, int, int, int);
void freeparser(Parser*);
uint32_t getdoc(char **buf, Parser *parser, FILE *fp);
void __print_doc_buf(char *buf, uint32_t b);
