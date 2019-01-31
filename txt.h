#define KB 1024
#define MB 1048576
#define ASCII 128
#define CR 13
#define LF 10
#define BUFSIZE 10240
#define MINTERMLEN 1
#define MAXTERMLEN 59 /* Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch */
#define NHASH  4093

typedef struct Token Token;

enum {KEEPCASE, LOWERCASE};

typedef enum {CTRLCHAR = 1, PRINTCHAR, SEPCHAR} E_asciitype;

struct Token {
	char *str;
	uint32_t id;
	uint32_t l;
};

/* Token */

/* Token     *newtoken(uint8_t*, uint32_t, uint8_t, uint32_t); */
/* void      freetoken(void*); */
/* int       cmptokenstr(void*, void*); */
/* uint32_t  hashtokenstr(void*, uint32_t); */
