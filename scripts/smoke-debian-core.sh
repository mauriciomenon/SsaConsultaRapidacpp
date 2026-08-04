#!/usr/bin/env bash
set -euo pipefail

run_debian_smoke_core() {
  local repo_root="${1}"
  local db_path="${2}"
  local clean_requested="${3}"
  local preset="${4}"
  local open_requested="${5}"
  local db_path_explicit="${6:-true}"

  # shellcheck disable=SC1091
  source "${repo_root}/scripts/debian-paths.sh"
  local build_dir
  build_dir="$(debian_build_dir "${repo_root}" "${preset}")"

  export TMPDIR=/tmp
  export TMP=/tmp
  export TEMP=/tmp

  local source_db_exists="false"
  if [[ -f "${db_path}" ]]; then
    source_db_exists="true"
  elif [[ "${db_path_explicit}" == "true" ]]; then
    echo "Database file not found: ${db_path}" >&2
    return 1
  fi

  if [[ "${clean_requested}" == "true" ]]; then
    "${repo_root}/scripts/build-debian.sh"
  else
    "${repo_root}/scripts/lazy_scripts/build-debian.sh" --preset "${preset}"
  fi
  ctest --test-dir "${build_dir}" --output-on-failure

  local runtime_dir="${repo_root}/build/runtime/debian"
  local config_dir="${runtime_dir}/config"
  local runtime_db="${runtime_dir}/ssas.db"
  local screenshot="${runtime_dir}/main.png"
  local preferences_source="${repo_root}/config/ssa_cpp_preferences.json.example"
  local executable
  executable="$(debian_app_executable "${repo_root}" "${preset}")"

  if [[ ! -x "${executable}" ]]; then
    echo "Binary not found after build: ${executable}" >&2
    return 1
  fi
  if [[ ! -f "${preferences_source}" ]]; then
    echo "Smoke preferences template not found: ${preferences_source}" >&2
    return 1
  fi

  mkdir -p "${runtime_dir}" "${config_dir}"
  rm -f "${runtime_db}"
  if [[ "${source_db_exists}" == "true" ]]; then
    cp "${db_path}" "${runtime_db}"
  fi
  cp "${preferences_source}" "${config_dir}/ssa_cpp_preferences.json"
  rm -f "${screenshot}"

  QT_QPA_PLATFORM=offscreen "${executable}" \
    --project-root "${repo_root}" \
    --db "${runtime_db}" \
    --config-dir "${config_dir}" \
    --screenshot "${screenshot}"

  if [[ ! -s "${screenshot}" ]]; then
    echo "Smoke screenshot was not produced: ${screenshot}" >&2
    return 1
  fi

  echo "Debian smoke screenshot: ${screenshot}"
  if [[ "${open_requested}" == "true" ]]; then
    "${executable}" \
      --project-root "${repo_root}" \
      --db "${runtime_db}" \
      --config-dir "${config_dir}"
  fi
}
