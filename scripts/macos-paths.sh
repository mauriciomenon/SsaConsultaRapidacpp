#!/usr/bin/env bash
set -euo pipefail

# Shared path contract for macOS run and smoke scripts.
# Keep this file free of build, test, and launch side effects.

macos_path_script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
# shellcheck source=project-paths.sh
source "${macos_path_script_dir}/project-paths.sh"

macos_default_runtime_dir() {
  local project_root="${1}"
  printf '%s\n' "${project_root}/build/runtime/macos"
}

macos_default_config_dir() {
  local runtime_dir="${1}"
  printf '%s\n' "${runtime_dir}/config"
}

macos_default_screenshot_path() {
  local runtime_dir="${1}"
  printf '%s\n' "${runtime_dir}/main.png"
}

macos_preferences_filename() {
  printf '%s\n' "ssa_cpp_preferences.json"
}

macos_app_executable() {
  local project_root="${1}"
  local preset="${2}"

  printf '%s\n' "${project_root}/build/${preset}/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida"
}

macos_cli_executable() {
  local project_root="${1}"
  local preset="${2}"

  printf '%s\n' "${project_root}/build/${preset}/ssa_consulta_rapida_cli"
}

print_macos_run_paths() {
  local project_root="${1}"
  local preset="${2}"
  local db_path="${3}"
  local config_dir="${4}"
  local runtime_dir="${5}"
  local screenshot="${6}"
  local arch
  arch="$(uname -m)"

  echo "SSA Consulta Rapida Cpp paths:" >&2
  echo "  project_root: ${project_root}" >&2
  echo "  gui_binary: $(macos_app_executable "${project_root}" "${preset}")" >&2
  echo "  cli_binary: $(macos_cli_executable "${project_root}" "${preset}")" >&2
  echo "  db_path: ${db_path}" >&2
  if [[ -n "${config_dir}" ]]; then
    echo "  config_dir: ${config_dir}" >&2
  fi
  if [[ -n "${runtime_dir}" ]]; then
    echo "  runtime_dir: ${runtime_dir}" >&2
  fi
  if [[ -n "${screenshot}" ]]; then
    echo "  screenshot: ${screenshot}" >&2
  fi
  echo "  build_dir: ${project_root}/build/${preset}" >&2
  echo "  package_dir_after_packaging: ${project_root}/dist/macos/${arch}" >&2
}
