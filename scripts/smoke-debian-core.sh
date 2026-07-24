#!/usr/bin/env bash
set -euo pipefail

run_debian_smoke_core() {
  local repo_root="${1}"
  local db_path="${2}"
  local clean_requested="${3}"
  local preset="${4}"
  local open_requested="${5}"

  export TMPDIR=/tmp
  export TMP=/tmp
  export TEMP=/tmp

  if [[ ! -f "${db_path}" ]]; then
    echo "Database file not found: ${db_path}" >&2
    return 1
  fi

  if [[ "${clean_requested}" == "true" ]]; then
    "${repo_root}/scripts/build-debian.sh"
  else
    "${repo_root}/scripts/lazy_scripts/build-debian.sh" --preset "${preset}"
  fi
  (cd "${repo_root}" && ctest --preset "${preset}" --output-on-failure)

  local runtime_dir="${repo_root}/build/runtime/debian"
  local config_dir="${runtime_dir}/config"
  local runtime_db="${runtime_dir}/ssas.db"
  local screenshot="${runtime_dir}/main.png"
  local preferences_source="${repo_root}/config/ssa_cpp_preferences.json.example"
  local executable="${repo_root}/build/${preset}/ssa_consulta_rapida"

  if [[ ! -x "${executable}" ]]; then
    echo "Binary not found after build: ${executable}" >&2
    return 1
  fi
  if [[ ! -f "${preferences_source}" ]]; then
    echo "Smoke preferences template not found: ${preferences_source}" >&2
    return 1
  fi

  mkdir -p "${runtime_dir}" "${config_dir}"
  cp "${db_path}" "${runtime_db}"
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
