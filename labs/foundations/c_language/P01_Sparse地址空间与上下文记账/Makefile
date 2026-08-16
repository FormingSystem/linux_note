SPARSE ?= sparse
SPARSE_FLAGS ?= -Wall -Wcontext
CC ?= gcc
SOURCE := sparse_annotation_demo.c

.PHONY: all check-good check-address-space check-context check-all \
	preprocess-sparse preprocess-compiler clean

all: check-good

check-good:
	$(SPARSE) $(SPARSE_FLAGS) $(SOURCE)

check-address-space:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_ADDRESS_SPACE $(SOURCE)

check-context:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_CONTEXT $(SOURCE)

check-all:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_ADDRESS_SPACE -DBAD_CONTEXT $(SOURCE)

preprocess-sparse:
	$(CC) -E -P -D__CHECKER__ $(SOURCE)

preprocess-compiler:
	$(CC) -E -P $(SOURCE)

clean:
	@:
