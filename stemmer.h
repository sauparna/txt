typedef struct stemmer Stemmer;

Stemmer *create_stemmer(void);
void free_stemmer(Stemmer*);
int stem(Stemmer*, char*, int);
