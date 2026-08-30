#!/usr/bin/env bash

typora_environment_init() {
    if [[ "$#" -ne 1 ]]; then
        printf '%s\n' '[typora] typora_environment_init requires the tools/typora directory.' >&2
        return 2
    fi

    TYPORA_TOOLS_ROOT="$(cd "$1" && pwd -P)"
    case "$(uname -s 2>/dev/null || true)" in
        MSYS_NT*|MINGW*|CYGWIN*)
            if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
                printf '[typora] Unsupported Windows Bash environment: %s; open MSYS2 UCRT64.\n' \
                    "${MSYSTEM:-unknown}" >&2
                return 1
            fi
            command -v cygpath >/dev/null 2>&1 || {
                printf '%s\n' '[typora] MSYS2 cygpath is unavailable.' >&2
                return 1
            }
            TYPORA_PLATFORM_FAMILY='msys2'
            TYPORA_PLATFORM_ID='windows-ucrt64'
            if [[ -z "${APPDATA:-}" ]]; then
                printf '%s\n' '[typora] APPDATA is unavailable; Typora user data cannot be located.' >&2
                return 1
            fi
            TYPORA_USER_DATA="$(cygpath -u -a "$APPDATA")/Typora"
            ;;
        Linux*)
            TYPORA_PLATFORM_FAMILY='linux'
            TYPORA_PLATFORM_ID='linux'
            if [[ -n "${XDG_CONFIG_HOME:-}" ]]; then
                TYPORA_USER_DATA="${XDG_CONFIG_HOME%/}/Typora"
            elif [[ -n "${HOME:-}" ]]; then
                TYPORA_USER_DATA="${HOME%/}/.config/Typora"
            else
                printf '%s\n' '[typora] HOME and XDG_CONFIG_HOME are unavailable.' >&2
                return 1
            fi
            ;;
        *)
            printf '[typora] Unsupported platform: %s.\n' "$(uname -s 2>/dev/null || printf unknown)" >&2
            return 1
            ;;
    esac

    export TYPORA_TOOLS_ROOT TYPORA_PLATFORM_FAMILY TYPORA_PLATFORM_ID TYPORA_USER_DATA
}

typora_normalize_input_path() {
    if [[ "$#" -ne 1 || -z "${1:-}" ]]; then
        return 1
    fi
    local input_path="$1"
    input_path="${input_path#\"}"
    input_path="${input_path%\"}"
    input_path="${input_path#\'}"
    input_path="${input_path%\'}"

    if [[ "$TYPORA_PLATFORM_FAMILY" == 'msys2' ]]; then
        if [[ "$input_path" =~ ^[A-Za-z]:[\\/] || "$input_path" == *\\* ]]; then
            cygpath -u -a "$input_path"
            return
        fi
        if [[ "$input_path" == /* ]]; then
            printf '%s\n' "$input_path"
            return
        fi
        printf '%s/%s\n' "$PWD" "$input_path"
        return
    fi

    if [[ "$input_path" =~ ^[A-Za-z]:[\\/] || "$input_path" == *\\* ]]; then
        printf '[typora] Windows path syntax is only accepted in MSYS2 UCRT64: %s\n' "$input_path" >&2
        return 1
    fi
    if [[ "$input_path" == /* ]]; then
        printf '%s\n' "$input_path"
    else
        printf '%s/%s\n' "$PWD" "$input_path"
    fi
}

typora_root_from_candidate() {
    local candidate_path candidate_name candidate_root
    candidate_path="$(typora_normalize_input_path "$1")" || return 1
    [[ -e "$candidate_path" ]] || return 1

    if [[ -f "$candidate_path" ]]; then
        candidate_name="$(basename "$candidate_path")"
        case "$candidate_name" in
            window.html)
                [[ "$(basename "$(dirname "$candidate_path")")" == 'resources' ]] || return 1
                candidate_root="$(dirname "$(dirname "$candidate_path")")"
                ;;
            Typora.exe|Typora|typora)
                candidate_root="$(dirname "$candidate_path")"
                ;;
            *) return 1 ;;
        esac
    elif [[ -f "$candidate_path/resources/window.html" ]]; then
        candidate_root="$candidate_path"
    elif [[ "$(basename "$candidate_path")" == 'resources' && -f "$candidate_path/window.html" ]]; then
        candidate_root="$(dirname "$candidate_path")"
    else
        return 1
    fi

    candidate_root="$(cd "$candidate_root" && pwd -P)" || return 1
    [[ -f "$candidate_root/resources/window.html" ]] || return 1
    if [[ "$TYPORA_PLATFORM_FAMILY" == 'msys2' && ! -f "$candidate_root/Typora.exe" ]]; then
        return 1
    fi
    printf '%s\n' "$candidate_root"
}

typora_running_executable() {
    if [[ "$TYPORA_PLATFORM_FAMILY" == 'msys2' ]]; then
        command -v powershell.exe >/dev/null 2>&1 || return 1
        powershell.exe -NoProfile -Command \
            '$process = Get-Process Typora -ErrorAction SilentlyContinue | Where-Object Path | Select-Object -First 1; if ($process) { $process.Path }' \
            2>/dev/null | tr -d '\r' | sed -n '1p'
        return
    fi

    local process_id
    process_id="$(pgrep -x Typora 2>/dev/null | sed -n '1p')"
    [[ -n "$process_id" ]] || process_id="$(pgrep -x typora 2>/dev/null | sed -n '1p')"
    [[ -n "$process_id" ]] || return 1
    readlink -f "/proc/$process_id/exe" 2>/dev/null
}

typora_resolve_root() {
    local requested_root="${1:-}"
    local non_interactive="${2:-0}"
    local candidate_path resolved_root command_name entered_path

    if [[ -n "$requested_root" ]]; then
        resolved_root="$(typora_root_from_candidate "$requested_root")" || {
            printf '[typora] The supplied path is not a valid Typora installation: %s\n' "$requested_root" >&2
            return 1
        }
        printf '%s\n' "$resolved_root"
        return
    fi

    if [[ -n "${TYPORA_ROOT:-}" ]]; then
        resolved_root="$(typora_root_from_candidate "$TYPORA_ROOT")" || {
            printf '[typora] TYPORA_ROOT does not identify a valid installation: %s\n' "$TYPORA_ROOT" >&2
            return 1
        }
        printf '%s\n' "$resolved_root"
        return
    fi

    candidate_path="$(typora_running_executable 2>/dev/null || true)"
    if [[ -n "$candidate_path" ]]; then
        resolved_root="$(typora_root_from_candidate "$candidate_path" 2>/dev/null || true)"
        if [[ -n "$resolved_root" ]]; then
            printf '%s\n' "$resolved_root"
            return
        fi
    fi

    for command_name in typora Typora Typora.exe; do
        candidate_path="$(command -v "$command_name" 2>/dev/null || true)"
        [[ -n "$candidate_path" ]] || continue
        if command -v readlink >/dev/null 2>&1; then
            candidate_path="$(readlink -f "$candidate_path" 2>/dev/null || printf '%s' "$candidate_path")"
        fi
        resolved_root="$(typora_root_from_candidate "$candidate_path" 2>/dev/null || true)"
        if [[ -n "$resolved_root" ]]; then
            printf '%s\n' "$resolved_root"
            return
        fi
    done

    if [[ "$non_interactive" == '1' ]]; then
        printf '%s\n' '[typora] Typora was not discovered. Use --typora-root or set TYPORA_ROOT.' >&2
        return 1
    fi

    printf '%s' '[typora] Typora was not discovered. Enter the installation directory, executable, resources directory, or resources/window.html: ' >&2
    if [[ -r /dev/tty ]]; then
        IFS= read -r entered_path </dev/tty
    else
        IFS= read -r entered_path
    fi
    [[ -n "$entered_path" ]] || {
        printf '%s\n' '[typora] No Typora path was provided.' >&2
        return 1
    }
    resolved_root="$(typora_root_from_candidate "$entered_path")" || {
        printf '[typora] The entered path is not a valid Typora installation: %s\n' "$entered_path" >&2
        return 1
    }
    printf '%s\n' "$resolved_root"
}

typora_sha256() {
    local file_path="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file_path" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file_path" | awk '{print $1}'
    elif command -v openssl >/dev/null 2>&1; then
        openssl dgst -sha256 "$file_path" | awk '{print $NF}'
    else
        printf '%s\n' '[typora] No SHA-256 implementation was found.' >&2
        return 1
    fi
}

typora_copy_file() {
    local source_path="$1"
    local target_path="$2"
    if [[ -w "$target_path" ]] || [[ ! -e "$target_path" && -w "$(dirname "$target_path")" ]]; then
        cp -f -- "$source_path" "$target_path"
        return
    fi
    if [[ "$TYPORA_PLATFORM_FAMILY" == 'linux' ]] && command -v sudo >/dev/null 2>&1; then
        printf '[typora] Administrator permission is required to update %s.\n' "$target_path" >&2
        sudo cp -f -- "$source_path" "$target_path"
        return
    fi
    printf '[typora] Target is not writable: %s\n' "$target_path" >&2
    return 1
}

typora_manifest_put() {
    local manifest_path="$1"
    local key="$2"
    local value="$3"
    printf '%s\t%s\n' "$key" "$(printf '%s' "$value" | base64 | tr -d '\r\n')" >> "$manifest_path"
}

typora_manifest_get() {
    local manifest_path="$1"
    local key="$2"
    local encoded_value
    encoded_value="$(awk -F '\t' -v requested_key="$key" '$1 == requested_key { print $2; exit }' "$manifest_path")"
    [[ -n "$encoded_value" ]] || return 1
    printf '%s' "$encoded_value" | base64 --decode
}
