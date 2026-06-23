#!/usr/bin/env bash
set -euo pipefail

package_resolve_script_dir() {
  local path="$1"
  path="$(cd "$(dirname "${path}")" && pwd)/$(basename "${path}")"
  while [[ -L "${path}" ]]; do
    local link_dir
    link_dir="$(cd "$(dirname "${path}")" && pwd)"
    path="$(readlink "${path}")"
    if [[ "${path}" != /* ]]; then
      path="${link_dir}/${path}"
    fi
    path="$(cd "$(dirname "${path}")" && pwd)/$(basename "${path}")"
  done
  printf '%s\n' "$(cd "$(dirname "${path}")" && pwd)"
}

package_repo_root_from_script() {
  local script_path="$1"
  local resolved_script_dir
  resolved_script_dir="$(package_resolve_script_dir "${script_path}")"
  printf '%s\n' "$(cd "${resolved_script_dir}/.." && pwd)"
}

package_project_version() {
  local repo_root="${1}"
  local version=""
  version="$(sed -nE \
    's/^[[:space:]]*project\(SSAConsultaRapidaCpp[[:space:]]+VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*$/\1/p' \
    "${repo_root}/CMakeLists.txt" | head -n 1)"
  printf '%s\n' "${version}"
}

package_set_latest_link() {
  local dist_root="$1"
  local artifact_name="$2"
  local latest_link="${dist_root}/latest"

  mkdir -p "${dist_root}"
  rm -rf "${latest_link}"
  ln -sfn "${artifact_name}" "${latest_link}"
}

package_set_latest_alias() {
  local dist_root="$1"
  local alias_name="$2"
  local target_relpath="$3"
  local latest_alias="${dist_root}/${alias_name}"

  mkdir -p "${dist_root}"
  rm -f "${latest_alias}"
  ln -sfn "${target_relpath}" "${latest_alias}"
}

package_linux_arch() {
  if command -v dpkg >/dev/null 2>&1; then
    dpkg --print-architecture
  else
    uname -m
  fi
}

package_resolve_macdeployqt() {
  local -a candidate_prefixes=()
  local candidate

  if command -v macdeployqt >/dev/null 2>&1; then
    printf '%s\n' "macdeployqt"
    return 0
  fi

  if [[ -n "${QT_DIR:-}" ]]; then
    candidate_prefixes+=("${QT_DIR}")
  fi
  if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    while IFS= read -r candidate; do
      [[ -n "${candidate}" ]] && candidate_prefixes+=("${candidate}")
    done < <(printf '%s\n' "${CMAKE_PREFIX_PATH}" | tr ':;' '\n')
  fi

  if command -v qtpaths >/dev/null 2>&1; then
    candidate="$(qtpaths --binary-dir 2>/dev/null)/macdeployqt"
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  fi

  candidate_prefixes+=(
    "/opt/homebrew/opt/qt"
    "/usr/local/opt/qt"
  )
  if [[ -f "${PWD}/tools/qt-detect.conf" ]]; then
    # shellcheck disable=SC1091
    source "${PWD}/tools/qt-detect.conf"
    candidate_prefixes+=("${MACOS_QT_ONLINE_INSTALLER_DIR}")
  fi
  if [[ -n "${QT_VERSION:-}" ]]; then
    candidate_prefixes+=("${HOME}/Qt/${QT_VERSION}/macos")
  fi

  for candidate in "${candidate_prefixes[@]}"; do
    candidate="${candidate%/}/bin/macdeployqt"
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

package_copy_runtime_libraries() {
  local binary="$1"
  local output_dir="$2"
  local lib_path
  local binary_dir
  local -a libs=()
  binary_dir="$(cd "$(dirname "${binary}")" && pwd)"

  if ! command -v ldd >/dev/null 2>&1; then
    return 0
  fi

  while IFS= read -r line; do
    if [[ "${line}" == *"=>"* ]]; then
      lib_path="${line#*=> }"
      lib_path="${lib_path%% *}"
      if [[ "${lib_path}" == "not" || "${lib_path}" == "not found" ]]; then
        continue
      fi
    elif [[ "${line}" == *"("* && "${line}" == *"/"* ]]; then
      lib_path="${line%% *}"
    else
      continue
    fi
    if [[ -z "${lib_path}" || "${lib_path}" == "not" ]]; then
      continue
    fi
    if [[ "${lib_path}" == /lib/* || "${lib_path}" == /lib64/* ]]; then
      case "${lib_path##*/}" in
        linux-vdso.so.1|ld-linux-*|libc.so.*|libdl.so.*|libm.so.*|libpthread.so.*|librt.so.*|libutil.so.*|libcrypt.so.*|libresolv.so.*|libnsl.so.*|libgcc_s.so.*|libstdc\+\+.so.*)
          continue
          ;;
        *)
          ;;
      esac
    fi
    if [[ "${lib_path}" == /* ]]; then
      :
    elif [[ -f "${binary_dir}/${lib_path}" ]]; then
      lib_path="${binary_dir}/${lib_path}"
    else
      continue
    fi
    if [[ -f "${lib_path}" ]]; then
      libs+=("${lib_path}")
    fi
  done < <(ldd "${binary}")

  if [[ "${#libs[@]}" -gt 0 ]]; then
    cp -f "${libs[@]}" "${output_dir}/"
  fi
}
