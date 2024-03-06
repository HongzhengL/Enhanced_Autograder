# Variables
CC=gcc
CFLAGS=-Wall -g

SRCDIR=src
INCDIR=include
LIBDIR=lib

SOL_DIR=solutions
PROJECT_NAME=project2

SOURCE_FILE=$(SRCDIR)/template.c
MQ_SRC_FILE=$(SRCDIR)/mq_template.c
N ?= 8
BINARIES=$(addprefix $(SOL_DIR)/sol_, $(shell seq 1 $(N)))
MQ_BINARIES=$(addprefix $(SOL_DIR)/mq_sol_, $(shell seq 1 $(N)))

# Default target
auto: autograder $(BINARIES)

mq_auto: mq_autograder $(MQ_BINARIES)

# Compile autograder
autograder: $(SRCDIR)/autograder.c $(LIBDIR)/utils.o
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $< $(LIBDIR)/utils.o 

# Compile mq_autograder
mq_autograder: $(SRCDIR)/mq_autograder.c $(LIBDIR)/utils.o
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $< $(LIBDIR)/utils.o 

# Compile utils.c into utils.o
$(LIBDIR)/utils.o: $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $< 

# Compile template.c into N binaries
$(SOL_DIR)/sol_%: $(SOURCE_FILE)
	mkdir -p $(SOL_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# Compile mq_template.c into N binaries
$(SOL_DIR)/mq_sol_%: $(MQ_SRC_FILE) $(LIBDIR)/utils.o
	mkdir -p $(SOL_DIR)
	$(CC) $(CFLAGS) -I${INCDIR} -o $@ $< $(LIBDIR)/utils.o

# Cases
exec: CFLAGS += -DEXEC
exec: auto

redir: CFLAGS += -DREDIR
redir: auto

pipe: CFLAGS += -DPIPE
pipe: auto

test1_exec: N=32
test1_exec: exec
	./autograder solutions 1 2 3

# Clean the build
clean:
	rm -f autograder mq_autograder
	rm -f solutions/sol_* solutions/mq_sol_*
	rm -f $(LIBDIR)/*.o
	rm -f input/*.in output/*
	rm -f test_results/*

zip:
	zip -r $(PROJECT_NAME).zip include lib src input output solutions expected Makefile README.md

test-setup:
	@chmod u+x testius
	@chmod -R u+x test_cases/
	@chmod -R u+x autograder
	rm -rf test_results/*

test_autograder: autograder test-setup
	@./testius test_cases/tests.json -v

test_mq_autograder: mq_autograder test-setup
	@./testius test_cases/mq_tests.json -v

kill:
	@for number in $(shell seq 1 $(N)); do \
		pgrep -f "sol_$$number" > /dev/null && (pkill -SIGKILL -f "sol_$$number" || echo "Could not kill sol_$$number") || true; \
	done

.PHONY: auto clean exec redir pipe zip test-setup test_autograder test_mq_autograder kill
