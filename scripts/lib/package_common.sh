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
  version="$(awk '
    $0 ~ /^project\(SSAConsultaRapidaCpp[[:space:]]+VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+/ {
      match($0, /VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+)/, m)
      print m[1]
      exit
    }
  ' "${repo_root}/CMakeLists.txt")"
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
  if command -v macdeployqt >/dev/null 2>&1; then
    printf '%s\n' "macdeployqt"
    return 0
  fi

  if command -v qtpaths >/dev/null 2>&1; then
    local candidate
    candidate="$(qtpaths --binary-dir 2>/dev/null)/macdeployqt"
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  fi

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
