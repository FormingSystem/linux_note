#!/usr/bin/env bash

set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PRACTICE_SOURCE_CONFIG="${PRACTICE_SOURCE_CONFIG:-$repo_dir/practice.sources.json}"
exec bash "$repo_dir/tools/practice_tool/start.sh" "$@"
