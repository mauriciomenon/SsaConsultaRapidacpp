#!/usr/bin/env bash
set -euo pipefail

# Debian/Linux executable path contract for run scripts.
# Build and packaging scripts remain responsible for creating these paths.

debian_build_arch() {
  case "$(uname -m)" in
    x86_64|amd64) printf '%s\n' amd64 ;;
    aarch64|arm64) printf '%s\n' arm64 ;;
    *)
      printf 'Unsupported Debian architecture: %s\n' "$(uname -m)" >&2
      return 1
      ;;
  esac
}

debian_build_dir() {
  local project_root="${1}"
  local preset="${2}"

  printf '%s\n' "${project_root}/build/debian/$(debian_build_arch)/${preset}"
}

debian_app_executable() {
  local project_root="${1}"
  local preset="${2}"

  printf '%s/ssa_consulta_rapida\n' "$(debian_build_dir "${project_root}" "${preset}")"
}
