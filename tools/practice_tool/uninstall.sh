#!/usr/bin/env bash

set -euo pipefail

practice_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$practice_dir/scripts/lib/platform_environment.sh"
practice_environment_init "$practice_dir"

mode=""
assume_yes=0

show_help() {
    cat <<'EOF'
practice uninstall - 清理训练工具生成的本机状态

用法：
  ./uninstall.sh --minimal [--yes]
  ./uninstall.sh --clean [--yes]

模式：
  --minimal  删除依赖、构建产物、就绪标记、日志和补全登记；保留运行环境与下载缓存
  --clean    在最小卸载基础上，只删除登记为 tool-owned 的运行环境和下载内容

安全边界：
  external 和 external-updated 软件永不由本卸载器删除。
  仓库内受 Git 管理的源码、配置、题库和文档不会被删除。
EOF
}

for argument in "$@"; do
    case "$argument" in
        --minimal) mode="minimal" ;;
        --clean) mode="clean" ;;
        --yes) assume_yes=1 ;;
        -h|--help) show_help; exit 0 ;;
        *) printf '[practice] 未知卸载参数：%s\n' "$argument" >&2; exit 2 ;;
    esac
done

if [[ -z "$mode" ]]; then
    printf '%s\n' \
        '[practice] 请选择卸载范围：' \
        '  [M] 最小卸载：保留运行环境和下载缓存' \
        '  [C] 干净卸载：同时删除工具自有运行环境和下载内容' \
        '----------------------------------------'
    read -r -p '[practice] 请输入 M 或 C：' answer
    case "${answer,,}" in
        m|minimal) mode="minimal" ;;
        c|clean) mode="clean" ;;
        *) printf '%s\n' '[practice] 已取消卸载。'; exit 0 ;;
    esac
fi

if [[ "$assume_yes" -ne 1 ]]; then
    printf '[practice] 即将执行%s卸载，是否继续？[y/N] ' "$([[ "$mode" = clean ]] && printf '干净' || printf '最小')"
    read -r confirmation
    case "${confirmation,,}" in y|yes) ;; *) printf '%s\n' '[practice] 已取消卸载。'; exit 0 ;; esac
fi

remove_completion() {
    [[ -n "${HOME:-}" ]] || return 0
    local completion_target="${XDG_DATA_HOME:-$HOME/.local/share}/bash-completion/completions/start.sh"
    local bashrc_path="$HOME/.bashrc"
    if [[ -L "$completion_target" && "$(readlink "$completion_target")" = "$practice_dir/scripts/completions/start.bash" ]]; then
        rm -f "$completion_target"
    elif [[ -f "$completion_target" ]] && grep -Fq "$practice_dir/scripts/completions/start.bash" "$completion_target"; then
        rm -f "$completion_target"
    fi
    if [[ -f "$bashrc_path" ]]; then
        local temporary
        temporary="$(mktemp)"
        awk '
            $0 == "# practice-tool start.sh completion" { skip = 1; next }
            skip == 1 { skip = 0; next }
            { print }
        ' "$bashrc_path" > "$temporary"
        mv "$temporary" "$bashrc_path"
    fi
}

printf '%s\n' '[practice] 正在清理工具生成状态……'
rm -rf "$practice_dir/node_modules" "$practice_dir/dist"
rm -f "$practice_dir"/.local/environment-ready-* "$practice_dir"/*.log
remove_completion

if [[ "$mode" = "clean" ]]; then
    registry="$PRACTICE_SOFTWARE_REGISTRY"
    local_runtime_owned=0
    cache_owned=0
    msys_cache_owned=0
    msys_root_pending=0
    msys_package=""
    if [[ -f "$registry" ]]; then
        while IFS=$'\t' read -r _ software_id _ _ _ ownership cleanup_kind _; do
            [[ "$ownership" = "tool-owned" ]] || continue
            case "$cleanup_kind" in
                local-runtime) local_runtime_owned=1 ;;
                download-cache)
                    if [[ "$software_id" = "msys2-installer-cache" ]]; then
                        msys_cache_owned=1
                    else
                        cache_owned=1
                    fi
                    ;;
                msys-package) msys_package="$software_id" ;;
                msys-root) msys_root_pending=1 ;;
            esac
        done < <(tail -n +2 "$registry")
    fi

    [[ "$local_runtime_owned" -eq 0 ]] || rm -rf "$PRACTICE_LOCAL_NODE_ROOT"
    [[ "$cache_owned" -eq 0 ]] || rm -rf "$PRACTICE_NODE_CACHE_ROOT"
    [[ "$msys_cache_owned" -eq 0 ]] || rm -rf "$practice_dir/.local/downloads/msys2"
    if [[ -n "$msys_package" && "$PRACTICE_PLATFORM_FAMILY" = "msys2" ]]; then
        expected_package="${MINGW_PACKAGE_PREFIX:-}-nodejs"
        if [[ "$msys_package" = "$expected_package" ]] && pacman -Q "$msys_package" >/dev/null 2>&1; then
            printf '[practice] 正在卸载工具从无到有安装的 MSYS2 包：%s\n' "$msys_package"
            pacman -Rns --noconfirm "$msys_package"
        fi
    fi
    rm -f "$practice_dir"/.local/environment-ready-*
    if [[ "$msys_root_pending" -eq 1 ]]; then
        printf '%s\n' '[practice] MSYS2 正在运行，无法从当前 Bash 删除。退出后请在 PowerShell 执行：'
        printf '  powershell -ExecutionPolicy Bypass -File "%s\\uninstall_windows.ps1" -Clean\n' \
            "$(cygpath -w "$practice_dir" 2>/dev/null || printf '%s' "$practice_dir")"
    else
        rm -f "$registry"
    fi
    rmdir "$PRACTICE_RUNTIME_ROOT" "$practice_dir/.local/downloads" "$practice_dir/.local" 2>/dev/null || true
fi

printf '%s\n' '[practice] 卸载完成。浏览器中的训练答案属于用户数据，未自动删除。'
