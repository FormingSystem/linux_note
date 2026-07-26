#!/usr/bin/env bash

# Shared runtime contract for Ubuntu 22.04 and MSYS2 UCRT64/UCRT32.
# Source this file, then call practice_environment_init <practice_tool_root>.

practice_environment_init() {
    if [[ "$#" -ne 1 ]]; then
        printf '%s\n' '[practice] practice_environment_init requires the practice_tool root.' >&2
        return 2
    fi

    PRACTICE_DIR="$1"
    PRACTICE_REQUIRED_NODE_MAJOR="${PRACTICE_REQUIRED_NODE_MAJOR:-18}"
    PRACTICE_PREFERRED_NODE_MAJORS="${PRACTICE_NODE_MAJORS:-24 22 20 18}"
    PRACTICE_NODE_DIST_SOURCES="${PRACTICE_NODE_DIST_SOURCES:-https://npmmirror.com/mirrors/node https://nodejs.org/dist}"
    PRACTICE_NPM_REGISTRIES="${PRACTICE_NPM_REGISTRIES:-https://registry.npmmirror.com https://registry.npmjs.org}"
    PRACTICE_RUNTIME_ROOT="$PRACTICE_DIR/.local/runtime"
    PRACTICE_LOCAL_NODE_ROOT="$PRACTICE_RUNTIME_ROOT/node"
    PRACTICE_LOCAL_NODE_BIN="$PRACTICE_LOCAL_NODE_ROOT/bin"
    PRACTICE_NODE_CACHE_ROOT="$PRACTICE_DIR/.local/downloads/node"
    PRACTICE_OFFLINE_NODE_TABLE="${PRACTICE_OFFLINE_NODE_TABLE:-$PRACTICE_DIR/config/offline_node_packages.local.tsv}"

    case "${MSYSTEM:-}" in
        UCRT64)
            PRACTICE_PLATFORM_FAMILY="msys2"
            PRACTICE_PLATFORM_ID="ucrt64"
            PRACTICE_PACKAGE_MANAGER="pacman"
            PRACTICE_NODE_ARCH="x64"
            ;;
        UCRT32)
            PRACTICE_PLATFORM_FAMILY="msys2"
            PRACTICE_PLATFORM_ID="ucrt32"
            PRACTICE_PACKAGE_MANAGER="pacman"
            PRACTICE_NODE_ARCH="x64"
            ;;
        "")
            if [[ ! -r /etc/os-release ]]; then
                printf '%s\n' '[practice] Unsupported Unix environment: /etc/os-release is unavailable.' >&2
                return 1
            fi
            # shellcheck disable=SC1091
            source /etc/os-release
            if [[ "${ID:-}" != "ubuntu" || "${VERSION_ID:-}" != "22.04" ]]; then
                printf '[practice] Unsupported Linux platform: %s %s; expected Ubuntu 22.04.\n' \
                    "${ID:-unknown}" "${VERSION_ID:-unknown}" >&2
                return 1
            fi
            PRACTICE_PLATFORM_FAMILY="ubuntu"
            PRACTICE_PLATFORM_ID="ubuntu-22.04"
            PRACTICE_PACKAGE_MANAGER="isolated-node-runtime"
            case "$(uname -m)" in
                x86_64|amd64) PRACTICE_NODE_ARCH="x64" ;;
                aarch64|arm64) PRACTICE_NODE_ARCH="arm64" ;;
                armv7l) PRACTICE_NODE_ARCH="armv7l" ;;
                *)
                    printf '[practice] Unsupported Ubuntu architecture: %s.\n' "$(uname -m)" >&2
                    return 1
                    ;;
            esac
            ;;
        *)
            printf '[practice] Unsupported MSYS2 environment: %s; expected UCRT64 or UCRT32.\n' "$MSYSTEM" >&2
            return 1
            ;;
    esac

    export PRACTICE_DIR PRACTICE_REQUIRED_NODE_MAJOR PRACTICE_PREFERRED_NODE_MAJORS
    export PRACTICE_NODE_DIST_SOURCES PRACTICE_NPM_REGISTRIES
    export PRACTICE_RUNTIME_ROOT PRACTICE_LOCAL_NODE_ROOT PRACTICE_LOCAL_NODE_BIN
    export PRACTICE_NODE_CACHE_ROOT PRACTICE_OFFLINE_NODE_TABLE
    export PRACTICE_PLATFORM_FAMILY PRACTICE_PLATFORM_ID PRACTICE_PACKAGE_MANAGER PRACTICE_NODE_ARCH
}

practice_command_exists() {
    command -v "$1" >/dev/null 2>&1
}

practice_activate_local_node() {
    if [[ -x "$PRACTICE_LOCAL_NODE_BIN/node" && -x "$PRACTICE_LOCAL_NODE_BIN/npm" ]]; then
        export PATH="$PRACTICE_LOCAL_NODE_BIN:$PATH"
        hash -r
    fi
}

practice_node_is_supported() {
    practice_command_exists node &&
        practice_command_exists npm &&
        [[ "$(node -p 'Number(process.versions.node.split(".")[0])' 2>/dev/null || printf '0')" -ge "$PRACTICE_REQUIRED_NODE_MAJOR" ]]
}

practice_resolve_tool_path() {
    local value="$1"
    if [[ "$value" = /* || "$value" =~ ^[A-Za-z]:[\\/].* ]]; then
        printf '%s\n' "$value"
    else
        printf '%s\n' "$PRACTICE_DIR/$value"
    fi
}

practice_prepare_download_tools() {
    if practice_command_exists curl; then
        PRACTICE_DOWNLOAD_TOOL="curl"
    elif practice_command_exists wget; then
        PRACTICE_DOWNLOAD_TOOL="wget"
    else
        printf '%s\n' '[practice] Downloading requires curl or wget.' >&2
        return 1
    fi
    if ! practice_command_exists sha256sum; then
        printf '%s\n' '[practice] SHA-256 verification requires sha256sum.' >&2
        return 1
    fi
    if ! practice_command_exists tar; then
        printf '%s\n' '[practice] Archive extraction requires tar.' >&2
        return 1
    fi
    PRACTICE_SHA256_TOOL="sha256sum"
    PRACTICE_TAR_TOOL="tar"
    export PRACTICE_DOWNLOAD_TOOL PRACTICE_SHA256_TOOL PRACTICE_TAR_TOOL
}

practice_download_file() {
    local url="$1"
    local destination="$2"
    case "$PRACTICE_DOWNLOAD_TOOL" in
        curl)
            curl --fail --location --retry 2 --connect-timeout 15 --progress-bar "$url" -o "$destination"
            ;;
        wget)
            wget --tries=2 --timeout=15 --show-progress "$url" -O "$destination"
            ;;
        *)
            printf '%s\n' '[practice] Download tools have not been initialized.' >&2
            return 2
            ;;
    esac
}

practice_verify_sha256_line() {
    local checksum_line="$1"
    local package_dir="$2"
    (
        cd "$package_dir"
        printf '%s\n' "$checksum_line" | "$PRACTICE_SHA256_TOOL" --check -
    )
}

practice_extract_tar() {
    local compression="$1"
    local archive_path="$2"
    local destination="$3"
    case "$compression" in
        gzip) "$PRACTICE_TAR_TOOL" -xzf "$archive_path" --strip-components=1 -C "$destination" ;;
        xz) "$PRACTICE_TAR_TOOL" -xJf "$archive_path" --strip-components=1 -C "$destination" ;;
        *)
            printf '[practice] Unsupported tar compression: %s.\n' "$compression" >&2
            return 2
            ;;
    esac
}

practice_install_msys2_node() {
    if [[ "$PRACTICE_PLATFORM_FAMILY" != "msys2" ]]; then
        return 2
    fi
    if ! practice_command_exists pacman; then
        printf '%s\n' '[practice] MSYS2 environment is missing pacman.' >&2
        return 1
    fi
    if [[ -z "${MINGW_PACKAGE_PREFIX:-}" ]]; then
        printf '[practice] %s does not define MINGW_PACKAGE_PREFIX.\n' "$PRACTICE_PLATFORM_ID" >&2
        return 1
    fi
    printf '[practice] 使用 MSYS2 pacman 安装 %s-nodejs（无需 sudo）……\n' "$MINGW_PACKAGE_PREFIX"
    pacman -S --needed --noconfirm "${MINGW_PACKAGE_PREFIX}-nodejs"
    hash -r
    practice_node_is_supported
}
