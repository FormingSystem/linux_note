#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'EOF'
用法：
  ./scripts/fix_mermaid_level_subgraphs.sh check
  ./scripts/fix_mermaid_level_subgraphs.sh fix

移除 Mermaid 中会导致 GitHub 和 VS Code 布局异常的 level0...levelN
伪层级子图。节点、连线、标签和 class/style 定义保持不变。
EOF
}

action=${1:-check}
case "$action" in
	check|fix) ;;
	-h|--help)
		usage
		exit 0
		;;
	*)
		usage >&2
		exit 2
		;;
esac

for command_name in git awk mktemp cmp; do
	command -v "$command_name" >/dev/null 2>&1 || {
		printf '缺少依赖：%s\n' "$command_name" >&2
		exit 127
	}
done

repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"

task_temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/mermaid-level-fix.XXXXXX")
cleanup() {
	case "$task_temp_dir" in
		"${TMPDIR:-/tmp}"/mermaid-level-fix.*) rm -rf -- "$task_temp_dir" ;;
		*) printf '拒绝清理非预期临时目录：%s\n' "$task_temp_dir" >&2 ;;
	esac
}
trap cleanup EXIT

changed_files=0
affected_blocks=0
removed_wrappers=0

while IFS= read -r -d '' path; do
	output_path="$task_temp_dir/output.md"
	stats_path="$task_temp_dir/stats"

	awk -v stats_path="$stats_path" '
		function logical_line(value) {
			sub(/\r$/, "", value)
			return value
		}

		BEGIN {
			in_mermaid = 0
			level_depth = 0
			block_changed = 0
			blocks = 0
			wrappers = 0
		}

		{
			logical = logical_line($0)

			if (!in_mermaid) {
				print $0
				if (logical ~ /^[[:space:]]*```mermaid[[:space:]]*$/) {
					in_mermaid = 1
					block_changed = 0
				}
				next
			}

			if (logical ~ /^[[:space:]]*```[[:space:]]*$/) {
				if (level_depth != 0) {
					print "Mermaid level 子图缺少 end" > "/dev/stderr"
					exit 3
				}
				print $0
				in_mermaid = 0
				if (block_changed) blocks++
				next
			}

			if (logical ~ /^[[:space:]]*subgraph level[0-9]+\[" "\][[:space:]]*$/) {
				level_depth++
				wrappers++
				block_changed = 1
				next
			}

			if (level_depth > 0 && logical ~ /^[[:space:]]*direction LR[[:space:]]*$/) {
				next
			}

			if (level_depth > 0 && logical ~ /^[[:space:]]*end[[:space:]]*$/) {
				level_depth--
				next
			}

			if (logical ~ /^[[:space:]]*style level[0-9]+ fill:transparent,stroke:transparent;?[[:space:]]*$/) {
				block_changed = 1
				next
			}

			print $0
		}

		END {
			if (in_mermaid || level_depth != 0) {
				print "Mermaid 代码块或 level 子图未闭合" > "/dev/stderr"
				exit 3
			}
			printf "%d %d\n", blocks, wrappers > stats_path
		}
	' "$path" > "$output_path"

	read -r file_blocks file_wrappers < "$stats_path"
	if ((file_blocks == 0)); then
		continue
	fi

	((changed_files += 1))
	((affected_blocks += file_blocks))
	((removed_wrappers += file_wrappers))
	printf '%s\n' "$path"

	if [[ $action == fix ]] && ! cmp -s -- "$path" "$output_path"; then
		mv -- "$output_path" "$path"
	fi
done < <(git ls-files -z -c -o --exclude-standard -- '*.md')

if [[ $action == check ]]; then
	printf '待修复：文件=%d Mermaid块=%d level子图=%d\n' \
		"$changed_files" "$affected_blocks" "$removed_wrappers"
	((changed_files == 0))
else
	printf '已修复：文件=%d Mermaid块=%d level子图=%d\n' \
		"$changed_files" "$affected_blocks" "$removed_wrappers"
fi
