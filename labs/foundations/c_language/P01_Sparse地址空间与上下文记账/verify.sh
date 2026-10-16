#!/usr/bin/env sh

set -eu

sparse_command=${SPARSE:-sparse}
sparse_flags=${SPARSE_FLAGS:--Wall -Wcontext}
source_file=sparse_annotation_demo.c

run_sparse()
{
	# 这里有意让SPARSE_FLAGS按空白拆成多个命令行参数。
	# shellcheck disable=SC2086
	"$sparse_command" $sparse_flags "$@" "$source_file" 2>&1
}

expect_clean()
{
	case_name=$1
	shift
	if output=$(run_sparse "$@"); then
		:
	else
		tool_status=$?
		printf 'FAIL %s: Sparse异常退出，状态=%s\n%s\n' \
			"$case_name" "$tool_status" "$output" >&2
		exit "$tool_status"
	fi
	if [ -n "$output" ]; then
		printf 'FAIL %s: 预期无诊断，实际输出如下：\n%s\n' "$case_name" "$output" >&2
		exit 1
	fi
	printf 'PASS %s: 未产生诊断\n' "$case_name"
}

expect_pattern()
{
	case_name=$1
	pattern=$2
	shift 2
	if output=$(run_sparse "$@"); then
		:
	else
		tool_status=$?
		printf 'FAIL %s: Sparse异常退出，状态=%s\n%s\n' \
			"$case_name" "$tool_status" "$output" >&2
		exit "$tool_status"
	fi
	if ! printf '%s\n' "$output" | grep -E -q "$pattern"; then
		printf 'FAIL %s: 没有观察到预期类别 %s，实际输出如下：\n%s\n' \
			"$case_name" "$pattern" "$output" >&2
		exit 1
	fi
	printf 'PASS %s: 观察到预期诊断类别\n' "$case_name"
}

command -v "$sparse_command" >/dev/null 2>&1 || {
	printf 'FAIL doctor: 找不到Sparse命令 %s\n' "$sparse_command" >&2
	exit 127
}

expect_clean good
expect_pattern address_boundary 'different address spaces' -DBAD_ADDRESS_BOUNDARY
expect_clean address_boundary_erased -DBAD_ADDRESS_BOUNDARY -DERASE_BOUNDARY_CONTRACT
expect_pattern address_assignment 'different address spaces' -DBAD_ADDRESS_ASSIGNMENT
expect_pattern noderef 'dereference of noderef expression' -DBAD_NODEREF
# Sparse 0.6.4对context(x,1,1)普通调用只传播净变化0。
# 这项静默是已知盲区，不表示反例满足持锁契约。
expect_clean context_call_gap -DBAD_CONTEXT_CALL
expect_pattern context_exit 'wrong count at exit' -DBAD_CONTEXT_EXIT
expect_pattern context_release 'unexpected unlock' -DBAD_CONTEXT_RELEASE
expect_pattern conditional_context \
	'different lock contexts for basic block|wrong count at exit' \
	-DBAD_CONDITIONAL_CONTEXT
expect_pattern wrapper_contract 'wrong count at exit' \
	-DERASE_LOCK_WRAPPER_CONTRACT

printf 'PASS verify: 所有正反例都满足当前实验的类别断言\n'
