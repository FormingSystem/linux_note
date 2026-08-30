#!/usr/bin/env bash
set -euo pipefail

typora_tools_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=scripts/lib/typora_environment.sh
source "$typora_tools_root/scripts/lib/typora_environment.sh"

requested_root=''
non_interactive=0
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --typora-root)
            [[ "$#" -ge 2 ]] || { printf '%s\n' '[typora] --typora-root requires a path.' >&2; exit 2; }
            requested_root="$2"
            shift 2
            ;;
        --non-interactive) non_interactive=1; shift ;;
        --help|-h)
            printf '%s\n' \
                'Usage: bash ./configure.sh [--typora-root PATH] [--non-interactive]' \
                'Supports Linux Bash and Windows MSYS2 UCRT64 Bash.' \
                'PATH may name the installation root, executable, resources directory, or resources/window.html.'
            exit 0
            ;;
        *) printf '[typora] Unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done

typora_environment_init "$typora_tools_root"
typora_root="$(typora_resolve_root "$requested_root" "$non_interactive")"
window_html="$typora_root/resources/window.html"
bundle_source="$typora_tools_root/enhancements/dist/typora_enhancements.js"
theme_source="$typora_tools_root/cpp_github-consolas.css"
theme_directory="$TYPORA_USER_DATA/themes"
theme_target="$theme_directory/cpp_github-consolas.css"
extension_directory="$TYPORA_USER_DATA/linux_note_enhancements"
bundle_target="$extension_directory/typora_enhancements.js"
timestamp="$(date '+%Y%m%d-%H%M%S')"
backup_root="$TYPORA_USER_DATA/backups/linux_note_typora_configuration/$timestamp"
manifest_path="$backup_root/configuration_manifest.tsv"
script_tag='<script defer src="typora://app/userData/linux_note_enhancements/typora_enhancements.js" data-linux-note-enhancements="true"></script>'

for required_path in "$window_html" "$bundle_source" "$theme_source"; do
    [[ -f "$required_path" ]] || { printf '[typora] Required file is missing: %s\n' "$required_path" >&2; exit 1; }
done
for marker in linux-note-vscode-textmate-c linux-note-vscode-textmate-cpp linux-note-mermaid-viewer linux-note-mermaid-inline-toolbar linux-note-code-collapsible linux-note-code-toggle is-code-collapsed mermaid_container_for_preview 'preview.prepend(toolbar)'; do
    grep -Fq "$marker" "$bundle_source" || { printf '[typora] Bundle marker is missing: %s\n' "$marker" >&2; exit 1; }
done
grep -Fq '</body>' "$window_html" || { printf '%s\n' '[typora] Typora resources/window.html does not contain </body>.' >&2; exit 1; }
command -v base64 >/dev/null 2>&1 || { printf '%s\n' '[typora] base64 is required.' >&2; exit 1; }

mkdir -p "$backup_root" "$theme_directory" "$extension_directory"
cp -- "$window_html" "$backup_root/window.html"
theme_existed=0
bundle_existed=0
if [[ -f "$theme_target" ]]; then
    theme_existed=1
    cp -- "$theme_target" "$backup_root/cpp_github-consolas.css"
fi
if [[ -f "$bundle_target" ]]; then
    bundle_existed=1
    cp -- "$bundle_target" "$backup_root/typora_enhancements.js"
fi

temporary_root="$(mktemp -d)"
new_window_html="$temporary_root/window.html"
sed \
    -e 's#<script defer src="typora://app/userData/linux_note_enhancements/typora_enhancements.js" data-linux-note-enhancements="true"></script>##g' \
    -e 's#</body>#<script defer src="typora://app/userData/linux_note_enhancements/typora_enhancements.js" data-linux-note-enhancements="true"></script></body>#' \
    "$window_html" > "$new_window_html"
entry_count="$(grep -oF "$script_tag" "$new_window_html" | wc -l | tr -d '[:space:]')"
[[ "$entry_count" == '1' ]] || { printf '[typora] Expected one enhancement entry, found %s.\n' "$entry_count" >&2; exit 1; }

configuration_started=0
configuration_committed=0
rollback_configuration() {
    local exit_code=$?
    set +e
    rm -f -- "$new_window_html"
    rmdir -- "$temporary_root" 2>/dev/null
    if [[ "$exit_code" -ne 0 && "$configuration_started" == '1' && "$configuration_committed" == '0' ]]; then
        printf '%s\n' '[typora] Configuration failed; restoring the pre-change files.' >&2
        typora_copy_file "$backup_root/window.html" "$window_html"
        if [[ "$theme_existed" == '1' ]]; then
            cp -f -- "$backup_root/cpp_github-consolas.css" "$theme_target"
        elif [[ -f "$theme_target" ]]; then
            mv -- "$theme_target" "$theme_target.disabled.$timestamp"
        fi
        if [[ "$bundle_existed" == '1' ]]; then
            cp -f -- "$backup_root/typora_enhancements.js" "$bundle_target"
        elif [[ -f "$bundle_target" ]]; then
            mv -- "$bundle_target" "$bundle_target.disabled.$timestamp"
        fi
    fi
    exit "$exit_code"
}
trap rollback_configuration EXIT

configuration_started=1
cp -f -- "$theme_source" "$theme_target"
cp -f -- "$bundle_source" "$bundle_target"
typora_copy_file "$new_window_html" "$window_html"

: > "$manifest_path"
typora_manifest_put "$manifest_path" schema_version '1'
typora_manifest_put "$manifest_path" configured_at "$(date -Iseconds)"
typora_manifest_put "$manifest_path" platform_id "$TYPORA_PLATFORM_ID"
typora_manifest_put "$manifest_path" typora_root "$typora_root"
typora_manifest_put "$manifest_path" window_html "$window_html"
typora_manifest_put "$manifest_path" theme_target "$theme_target"
typora_manifest_put "$manifest_path" bundle_target "$bundle_target"
typora_manifest_put "$manifest_path" theme_existed "$theme_existed"
typora_manifest_put "$manifest_path" bundle_existed "$bundle_existed"
typora_manifest_put "$manifest_path" window_sha256 "$(typora_sha256 "$window_html")"
typora_manifest_put "$manifest_path" theme_sha256 "$(typora_sha256 "$theme_target")"
typora_manifest_put "$manifest_path" bundle_sha256 "$(typora_sha256 "$bundle_target")"
configuration_committed=1

printf '%s\n' \
    '[typora] Configuration completed.' \
    "[typora] Platform: $TYPORA_PLATFORM_ID" \
    "[typora] Typora root: $typora_root" \
    "[typora] Unified backup: $backup_root" \
    '[typora] Save open documents, restart Typora, and select cpp github consolas.'
