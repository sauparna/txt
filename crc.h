/*
  crcany version 1.0, 22 December 2014

  Copyright (C) 2014 Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu
*/

/* Version history:
   1.0  22 Dec 2014  First version
 */

/* Generalized CRC algorithm.  Compute any specified CRC up to 128 bits long.
   Take model inputs from http://reveng.sourceforge.net/crc-catalogue/all.htm
   and verify the check values.  This verifies all 72 CRCs on that page (as of
   the version date above).  The lines on that page that start with "width="
   can be fed verbatim to this program.  The 128-bit limit assumes that
   uintmax_t is 64 bits.  The bit-wise algorithms here will compute CRCs up to
   a width twice that of the typedef'ed word_t type.

   This code also generates and tests table-driven algorithms for high-speed.
   The byte-wise algorithm processes one byte at a time instead of one bit at a
   time, and the word-wise algorithm ingests one word_t at a time.  The table
   driven algorithms here only work for CRCs that fit in a word_t, though they
   could be extended in the same way the bit-wise algorithm is extended here.

   The CRC parameters used in the linked catalogue were originally defined in
   Ross Williams' "A Painless Guide to CRC Error Detection Algorithms", which
   can be found here: http://zlib.net/crc_v3.txt .
 */

/* Type to use for CRC calculations.  This should be the largest unsigned
   integer type available, to maximize the cases that can be computed.  word_t
   can be any unsigned integer type, except for unsigned char.  All of the
   algorithms here can process CRCs up to the size of a word_t.  The bit-wise
   algorithm here can process CRCs up to twice the size of a word_t. */
/* typedef uintmax_t word_t; */

/* Determine the size of uintmax_t at pre-processor time.  (sizeof is not
   evaluated at pre-processor time.)  If word_t is instead set to an explicit
   size above, e.g. uint64_t, then #define WORDCHARS appropriately, e.g. as 8.
   WORDCHARS must be 2, 4, 8, or 16. */
/*
#if UINTMAX_MAX == UINT16_MAX
#  define WORDCHARS 2
#elif UINTMAX_MAX == UINT32_MAX
#  define WORDCHARS 4
#elif UINTMAX_MAX == UINT64_MAX
#  define WORDCHARS 8
#elif UINTMAX_MAX == UINT128_MAX
#  define WORDCHARS 16
#else
#  error uintmax_t must be 2, 4, 8, or 16 bytes for this code.
#endif
*/

typedef uint32_t word_t;
#define WORDCHARS 4

/* The number of bits in a word_t (assumes CHAR_BIT is 8). */
#define WORDBITS (WORDCHARS<<3)

/* Mask for the low n bits of a word_t (n must be greater than zero). */
#define ONES(n) (((word_t)0 - 1) >> (WORDBITS - (n)))

/* CRC description and tables, allowing for double-word CRCs.

   The description is based on Ross William's parameters, but with some changes
   to the parameters as described below.

   ref and rev are derived from refin and refout.  rev and rev must be 0 or 1.
   ref is the same as refin.  rev is true only if refin and refout are
   different.  rev true is very uncommon, and occurs in only one of the 72 CRCs
   in the RevEng catalogue.  When rev is false, the common case, ref true means
   that both the input and output are reflected. Reflected CRCs are quite
   common.

   init is different here as well, representing the CRC of a zero-length
   sequence, instead of the initial contents of the CRC register.

   poly is reflected for refin true.  xorout is reflected for refout true.

   The structure includes space for pre-computed CRC tables used to speed up
   the CRC calculation.  Both are filled in by the crc_table_wordwise()
   routine, using the CRC parameters already defined in the structure. */
typedef struct {
    unsigned short width;       /* number of bits in the CRC (the degree of the
                                   polynomial) */
    char ref;                   /* if true, reflect input and output */
    char rev;                   /* if true, reverse output */
    word_t poly, poly_hi;       /* polynomial representation (sans x^width) */
    word_t init, init_hi;       /* CRC of a zero-length sequence */
    word_t xorout, xorout_hi;   /* final CRC is exclusive-or'ed with this */
    word_t check, check_hi;     /* CRC of the nine ASCII bytes "12345679" */
    char *name;                 /* text description of this CRC */
    word_t table_byte[256];             /* table for byte-wise calculation */
    word_t table_word[WORDCHARS][256];  /* tables for word-wise calculation */
} model_t;

word_t reverse(word_t x, unsigned n);
word_t crc_bitwise(model_t *model, word_t crc,
				 unsigned char *buf, size_t len);
void crc_table_bytewise(model_t *model);
word_t crc_bytewise(model_t *model, word_t crc,
                                  unsigned char *buf, size_t len);  
word_t swap(word_t x);
void crc_table_wordwise(model_t *model);
word_t crc_wordwise(model_t *model, word_t crc,
				  unsigned char *buf, size_t len);
void reverse_dbl(word_t *hi, word_t *lo, unsigned n);
void crc_bitwise_dbl(model_t *model, word_t *crc_hi, word_t *crc_lo,
                            unsigned char *buf, size_t len);
int read_var(char **str, char **name, char **value);
char *strtobig(char *str, word_t *high, word_t *low);
int read_model(model_t *model, char *str);
uint32_t compute_crc(void *buf, size_t len);
