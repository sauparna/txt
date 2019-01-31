# subdirectory for objects
O=obj
TEST=test

CC = gcc
CFLAGS = -g -Wall -O0
INCLUDES = -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LIBS = -lk

DEPDIR = .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

SRC = \
	txt.c       \
	tokenizer.c \
	porter.c

OBJ = $(patsubst %.c,$(O)/%.o,$(SRC))

all: txt

txt: $(O)/txt.o $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $^ $(LIBS)

# raw2t: $(O)/raw2t.o $(OBJ)
# 	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $^ $(LIBS)

# t2mem: $(O)/t2mem.o $(OBJ)
# 	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $^ $(LIBS)

# ii: $(O)/ii.o $(OBJ)
# 	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $^ $(LIBS)

$(O)/%.o: %.c $(DEPDIR)/%.d
	$(CC) $(DEPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRC)))

# .PHONY: install
# install: libk.a
# 	mkdir -p $(PREFIX)/include/kak
# 	cp -p *.h $(PREFIX)/include/kak/
# 	cp -p libk.a $(PREFIX)/lib/

# .PHONY: uninstall
# uninstall:
# 	rm -f $(PREFIX)/lib/libk.a
# 	rm -f $(PREFIX)/include/kak/*.h

.PHONY: clean
clean:
	rm -f $(O)/*.o $(DEPDIR)/*.d txt

# .PHONY: parsetest
# parsetest:
# 	./raw2t -x -n -c TREC <test/alice.txt | ./t2mem | diff -q - test/alice.mem
# 	./raw2t -x -n -c TRECQUERY <test/alice_query.txt | ./t2mem | diff -q - test/alice_query.mem

# .PHONY: iitest
# iitest:
# 	./ii -s test/alice_query.t <test/alice.t | sort -k1,1 -k3,3nr | diff -q - test/alice.rank

# .PHONY: alice
# alice:
# 	./raw2t -x -n -c TREC <test/alice.txt >/tmp/alice.t
# 	./raw2t -x -n -c TRECQUERY <test/alice_query.txt >/tmp/alice_query.t
# 	./t2mem </tmp/alice.t >/tmp/alice.mem
# 	./t2mem </tmp/alice_query.t >/tmp/alice_query.mem
# 	./ii -s /tmp/alice_query.t </tmp/alice.t >/tmp/alice.res
# 	sort -k1,1 -k3,3nr /tmp/alice.res >/tmp/alice.rank
# 	awk -f txt2trecrun.awk </tmp/alice.rank >/tmp/alice.run
# 	~/ir/trec_eval.9.0/trec_eval -q test/alice.qrel /tmp/alice.run >/tmp/alice.eval

# .PHONY: ap
# ap:
# 	./raw2t -x -n -c TREC <test/ap.txt >/tmp/ap.t
# 	./raw2t -x -n -c TRECQUERY <test/ap_query.txt >/tmp/ap_query.t
# 	./t2mem </tmp/ap.t >/tmp/ap.mem
# 	./t2mem </tmp/ap_query.t >/tmp/ap_query.mem
# 	./ii -s /tmp/ap_query.t </tmp/ap.t >/tmp/ap.res
# 	sort -k1,1 -k3,3nr /tmp/ap.res >/tmp/ap.rank
# 	awk -f txt2trecrun.awk </tmp/ap.rank >/tmp/ap.run
# 	~/ir/trec_eval.9.0/trec_eval -q test/ap.qrel /tmp/ap.run >/tmp/ap.eval
