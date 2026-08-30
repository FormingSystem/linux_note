#!/usr/bin/env bash
set -euo pipefail

typora_tools_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=scripts/lib/typora_environment.sh
source "$typora_tools_root/scripts/lib/typora_environment.sh"

backup_input=''
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --backup-root) [[ "$#" -ge 2 ]] || exit 2; backup_input="$2"; shift 2 ;;
        --help|-h) printf '%s\n' 'Usage: bash ./restore_configuration.sh --backup-root PATH'; exit 0 ;;
        *) printf '[typora] Unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done
[[ -n "$backup_input" ]] || { printf '%s\n' '[typora] --backup-root is required.' >&2; exit 2; }

typora_environment_init "$typora_tools_root"
backup_root="$(typora_normalize_input_path "$backup_input")"
backup_root="$(cd "$backup_root" && pwd -P)" || { printf '%s\n' '[typora] Backup directory is unavailable.' >&2; exit 1; }
manifest_path="$backup_root/configuration_manifest.tsv"
window_backup="$backup_root/window.html"
[[ -f "$manifest_path" && -f "$window_backup" ]] || { printf '%s\n' '[typora] Backup manifest or window.html is missing.' >&2; exit 1; }

schema_version="$(typora_manifest_get "$manifest_path" schema_version)"
[[ "$schema_version" == '1' ]] || { printf '[typora] Unsupported backup schema: %s\n' "$schema_version" >&2; exit 1; }
typora_root="$(typora_manifest_get "$manifest_path" typora_root)"
window_html="$(typora_manifest_get "$manifest_path" window_html)"
theme_target="$(typora_manifest_get "$manifest_path" theme_target)"
bundle_target="$(typora_manifest_get "$manifest_path" bundle_target)"
theme_existed="$(typora_manifest_get "$manifest_path" theme_existed)"
bundle_existed="$(typora_manifest_get "$manifest_path" bundle_existed)"

expected_window="$typora_root/resources/window.html"
expected_theme="$TYPORA_USER_DATA/themes/cpp_github-consolas.css"
expected_bundle="$TYPORA_USER_DATA/linux_note_enhancements/typora_enhancements.js"
[[ "$window_html" == "$expected_window" && "$theme_target" == "$expected_theme" && "$bundle_target" == "$expected_bundle" ]] || {
    printf '%s\n' '[typora] Backup targets do not match the current validated platform paths; restore stopped.' >&2
    exit 1
}
[[ -f "$window_html" ]] || { printf '[typora] Current Typora entry is missing: %s\n' "$window_html" >&2; exit 1; }
if [[ "$theme_existed" == '1' && ! -f "$backup_root/cpp_github-consolas.css" ]]; then
    printf '%s\n' '[typora] Recorded theme backup is missing.' >&2
    exit 1
fi
if [[ "$bundle_existed" == '1' && ! -f "$backup_root/typora_enhancements.js" ]]; then
    printf '%s\n' '[typora] Recorded bundle backup is missing.' >&2
    exit 1
fi

timestamp="$(date '+%Y%m%d-%H%M%S')"
cp -- "$window_html" "$backup_root/window.before_restore.$timestamp.html"
typora_copy_file "$window_backup" "$window_html"
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

printf '%s\n' \
    '[typora] Configuration restored.' \
    "[typora] Backup source: $backup_root" \
    '[typora] Save open documents and restart Typora.'
