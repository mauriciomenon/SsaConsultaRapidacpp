#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
# shellcheck source=macos-paths.sh
source "${script_dir}/macos-paths.sh"

run_macos_app() {
  local project_root="${1}"
  local preset="${2}"
  local db_path="${3}"
  local config_dir="${4}"
  local screenshot="${5}"
  local launch_mode="${6:-open}"
  local runtime_dir="${7:-}"
  local executable
  executable="$(macos_app_executable "${project_root}" "${preset}")"

  if [[ ! -x "${executable}" ]]; then
    echo "Binary not found: ${executable}" >&2
    return 1
  fi

  print_macos_run_paths "${project_root}" "${preset}" "${db_path}" "${config_dir}" "${runtime_dir}" \
    "${screenshot}"

  local args=(--project-root "${project_root}" --db "${db_path}")
  if [[ -n "${config_dir}" ]]; then
    args+=(--config-dir "${config_dir}")
  fi

  case "${launch_mode}" in
    screenshot)
      if [[ -z "${screenshot}" ]]; then
        echo "Screenshot path is required in screenshot launch mode." >&2
        return 2
      fi
      QT_QPA_PLATFORM=offscreen "${executable}" "${args[@]}" --screenshot "${screenshot}"
      ;;
    open)
      "${executable}" "${args[@]}"
      ;;
    *)
      echo "Unknown macOS launch mode: ${launch_mode}" >&2
      return 2
      ;;
  esac
}

prepare_macos_build_dir() {
  local project_root="${1}"
  local preset="${2}"
  local clean_requested="${3}"
  local build_dir="${project_root}/build/${preset}"
  local configure_script="${project_root}/tools/configure-dev.sh"

  if [[ -z "${CMAKE_PREFIX_PATH}" ]]; then
    export CMAKE_PREFIX_PATH="${HOME}/Qt/6.11.0/macos"
  fi

  if [[ "${clean_requested}" == "true" ]]; then
    rm -rf "${build_dir}"
    "${configure_script}" "${preset}"
  elif [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
    "${configure_script}" "${preset}"
  fi
}

run_macos_smoke_core() {
  local project_root="${1}"
  local preset="${2}"
  local db_path="${3}"
  local runtime_dir="${4}"
  local config_dir="${5}"
  local screenshot="${6}"
  local clean_requested="${7}"
  local preferences_src="${8}"
  local launch_mode="${9:-screenshot}"

  prepare_macos_build_dir "${project_root}" "${preset}" "${clean_requested}"
  if ! cmake --build --preset "${preset}"; then
    echo "Build failed for preset: ${preset}" >&2
    return 1
  fi
  if ! ctest --preset "${preset}" --output-on-failure; then
    echo "Tests failed for preset: ${preset}" >&2
    return 1
  fi

  if [[ ! -f "${db_path}" ]]; then
    echo "Database file not found: ${db_path}" >&2
    return 1
  fi
  mkdir -p "${runtime_dir}" "${config_dir}"
  cp "${db_path}" "${runtime_dir}/ssas.db"
  if [[ -f "${preferences_src}" ]]; then
    cp "${preferences_src}" "${config_dir}/$(macos_preferences_filename)"
  fi

  run_macos_app "${project_root}" "${preset}" "${runtime_dir}/ssas.db" "${config_dir}" \
    "${screenshot}" "${launch_mode}" "${runtime_dir}"
}
