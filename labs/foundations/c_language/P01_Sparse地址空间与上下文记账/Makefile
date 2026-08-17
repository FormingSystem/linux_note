SPARSE ?= sparse
SPARSE_FLAGS ?= -Wall -Wcontext
CC := gcc
CC_FLAGS ?= -std=gnu11 -Wall -Wextra -Werror -fsyntax-only
SOURCE := sparse_annotation_demo.c
ALL_BAD_FLAGS := \
	-DBAD_ADDRESS_BOUNDARY \
	-DBAD_ADDRESS_ASSIGNMENT \
	-DBAD_NODEREF \
	-DBAD_CONTEXT_CALL \
	-DBAD_CONTEXT_EXIT \
	-DBAD_CONTEXT_RELEASE \
	-DBAD_CONDITIONAL_CONTEXT

.PHONY: all doctor compile-compiler check-good check-address-boundary \
	check-address-boundary-erased check-address-assignment check-noderef \
	check-context-call check-context-exit check-context-release \
	check-conditional-context check-wrapper-contract check-all verify \
	preprocess-sparse preprocess-compiler clean

all: check-good

doctor:
	@command -v $(SPARSE)
	@$(SPARSE) --version
	@command -v $(CC)
	@$(CC) --version
	@$(MAKE) --version

compile-compiler:
	$(CC) $(CC_FLAGS) $(ALL_BAD_FLAGS) $(SOURCE)

check-good:
	$(SPARSE) $(SPARSE_FLAGS) $(SOURCE)

check-address-boundary:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_ADDRESS_BOUNDARY $(SOURCE)

check-address-boundary-erased:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_ADDRESS_BOUNDARY \
		-DERASE_BOUNDARY_CONTRACT $(SOURCE)

check-address-assignment:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_ADDRESS_ASSIGNMENT $(SOURCE)

check-noderef:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_NODEREF $(SOURCE)

check-context-call:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_CONTEXT_CALL $(SOURCE)

check-context-exit:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_CONTEXT_EXIT $(SOURCE)

check-context-release:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_CONTEXT_RELEASE $(SOURCE)

check-conditional-context:
	$(SPARSE) $(SPARSE_FLAGS) -DBAD_CONDITIONAL_CONTEXT $(SOURCE)

check-wrapper-contract:
	$(SPARSE) $(SPARSE_FLAGS) -DERASE_LOCK_WRAPPER_CONTRACT $(SOURCE)

check-all:
	$(SPARSE) $(SPARSE_FLAGS) $(ALL_BAD_FLAGS) $(SOURCE)

verify: compile-compiler
	sh ./verify.sh

preprocess-sparse:
	$(CC) -E -P -D__CHECKER__ $(SOURCE)

preprocess-compiler:
	$(CC) -E -P $(SOURCE)

clean:
	@:
