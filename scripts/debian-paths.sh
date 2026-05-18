#!/usr/bin/env bash
set -euo pipefail

# Debian/Linux executable path contract for run scripts.
# Build and packaging scripts remain responsible for creating these paths.

debian_app_executable() {
  local project_root="${1}"
  local preset="${2}"

  printf '%s\n' "${project_root}/build/${preset}/ssa_consulta_rapida"
}
