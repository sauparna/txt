#define NTERM 10000    
#define BTHEADER 34    /* packed bytes for THeader; 1*2 + 4*8 */
#define THEADER 34     /* bytes in memory */
#define BTDOCHEADER 42 /* packed bytes for TDocHeader; 1*2 + 4*6 + 16*1 */
#define TDOCHEADER 34  /* bytes in memory; 1*2 + 4*6 + 8*1 */

#define BFLOAT 16      /* a double formatted as +1.12345678e+01, using
			* format specifier +1.8e is 15 bytes long. One
			* extra byte for 3-digit exponent. (Why 3?)*/

typedef unsigned char uchar;

typedef struct TFile TFile;
typedef struct THeader THeader;
typedef struct TDocHeader TDocHeader;
typedef struct TDoc TDoc;

struct THeader {
	uint8_t  type;
	uint8_t  bo;
	uint32_t b;
	uint32_t nt;
	uint32_t nd;
	uint32_t bt;
	uint32_t bd;
	uint32_t sumb;
	uint32_t sumu;
	uint32_t sumtf;
};

struct TDocHeader {
	uint8_t  type;
	uint8_t  bo;
	uint32_t id;
	uint32_t sumb;
	uint32_t sumu;
	uint32_t sumtf;
	uint32_t maxtf;
	uint32_t sumsqtf;
	double   sumsqlogtf;
};

struct TDoc {
	TDocHeader *h;
	uint32_t   *txt;
	uint32_t   *tf;
};

struct TFile {
	THeader *h;
	Hash    *ht;
	Hash    *hd;
	Hash    *hdh;
	Node    *list;
};

THeader    *_newTHeader(void);
TDocHeader *_newTDocHeader(void);
int        cmpdoc(void*, void*);
uint32_t   hashdoc(void*, uint32_t);

TDoc *newTDoc(void);
void freeTDoc(void*);
void updateTDoc(void*, void*);

TFile *newTFile(void);
void  freeTFile(void*);

void _printTHeader(THeader*, FILE*);
void _printTDocHeader(TDocHeader*, FILE*);
void _printTDoc(TDoc*, FILE*);
void printTFile(TFile*, FILE*);

int _pack(uint8_t *buf, char *fmt, ...);
int _unpack(uint8_t *buf, char *fmt, ...);

int packTHeader(uint8_t *buf, THeader *h);
int pack_idmap(uint8_t *buf, Hash *h);
int packTDocHeader(uint8_t *buf, TDocHeader *h);
int packTDoc_payload(uint8_t *buf, TDoc *tdoc);

int unpackTHeader(THeader *h, int n, uint8_t *buf);
int unpack_idmap(Hash *h, int n, uint8_t *buf);
int unpackTDocHeader(TDocHeader *h, int n, uint8_t *buf);
int unpackTDoc_payload(TDoc *tdoc, int n, uint8_t *buf);

void writeTFile(FILE *fp, TFile *tfile);
TFile *readTFile(FILE *fp, TFile *tfile);
