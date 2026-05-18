#!/usr/bin/env bash
set -euo pipefail

# Shared project path contract for Bash entrypoints.
# This file only resolves stable repository paths; it does not build or launch.

resolve_project_default_db_path() {
  local project_root="${1}"
  local candidate="${project_root}/data/ssas.db"

  if [[ -f "${candidate}" ]]; then
    printf '%s\n' "${candidate}"
    return 0
  fi

  return 1
}

print_project_default_db_not_found() {
  local project_root="${1}"
  local explicit_path_hint="${2}"

  echo "Database file not found in the default location." >&2
  echo "Place a valid SQLite file at:" >&2
  echo "  ${project_root}/data/ssas.db" >&2
  echo "${explicit_path_hint}" >&2
}
