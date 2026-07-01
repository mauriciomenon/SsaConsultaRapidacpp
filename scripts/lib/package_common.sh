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
    /^[[:space:]]*project[[:space:]]*\(/ { in_project = 1 }
    in_project {
      for (i = 1; i <= NF; ++i) {
        token = $i
        gsub(/[()]/, "", token)
        if (want_version) {
          print token
          exit
        }
        if (token == "VERSION") {
          want_version = 1
        }
      }
      if ($0 ~ /\)/) {
        in_project = 0
      }
    }
  ' "${repo_root}/CMakeLists.txt" | head -n 1)"
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

  # PRIORIDADE: o macdeployqt (e os plugins que ele copia) devem vir da MESMA
  # Qt usada no build. Se misturarmos (ex.: build com 6.11.0 mas macdeployqt do
  # brew 6.11.1), o bundle fica com frameworks 6.11.0 e plugin cocoa 6.11.1 ->
  # mismatch de versao -> crash via launchd/Finder ("Could not load platform
  # plugin cocoa"). Por isso QT_DIR/CMAKE_PREFIX_PATH vencem o PATH generico.

  if [[ -n "${QT_DIR:-}" ]]; then
    candidate_prefixes+=("${QT_DIR}")
  fi
  if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    while IFS= read -r candidate; do
      [[ -n "${candidate}" ]] && candidate_prefixes+=("${candidate}")
    done < <(printf '%s\n' "${CMAKE_PREFIX_PATH}" | tr ':;' '\n')
  fi

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

  # Fallback: qtpaths, depois macdeployqt generico do PATH (brew, etc.).
  if command -v qtpaths >/dev/null 2>&1; then
    candidate="$(qtpaths --binary-dir 2>/dev/null)/macdeployqt"
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  fi

  if command -v macdeployqt >/dev/null 2>&1; then
    printf '%s\n' "macdeployqt"
    return 0
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
    # -L resolve symlinks (Qt envia libs versionadas como symlinks; cp sem -L
    # copiaria o link apontando para lugar inexistente no destino).
    cp -fL "${libs[@]}" "${output_dir}/"
  fi
}

# Descobre o prefix de instalacao do Qt6 a partir de um binario linkado contra
# Qt6. Prioridade: CMAKE_PREFIX_PATH/QT_DIR (fonte do build) antes de ldd e
# qmake do PATH (que pode apontar para uma Qt diferente da do build, causando
# mismatch de versao nos plugins copiados). Ecoa o prefix para stdout.
package_resolve_qt_prefix() {
  local binary="$1"
  local candidate

  # Caminho 1: vars de ambiente que controlaram o build (mais confiavel).
  for candidate in "${CMAKE_PREFIX_PATH:-}" "${QT_DIR:-}"; do
    candidate="${candidate%%:*}"
    if [[ -n "${candidate}" && -d "${candidate}/plugins" ]]; then
      echo "${candidate}"
      return 0
    fi
  done

  # Caminho 2: ldd revela onde libQt6Core.so.6 mora.
  local qt_core
  if qt_core="$(ldd "${binary}" 2>/dev/null | grep -Eo '/[^ ]*libQt6Core\.so[^ ]*' | head -n1)"; then
    if [[ -n "${qt_core}" ]]; then
      # lib pode estar em <prefix>/lib, <prefix>/lib/<multiarch>, ou
      # <prefix>/lib/qt6 (Debian/Arch). Sobe e procura plugins/qml.
      local lib_dir
      lib_dir="$(cd "$(dirname "${qt_core}")" && pwd)"
      local parent
      parent="$(cd "${lib_dir}/.." && pwd)"
      for candidate in "${parent}" "${lib_dir}/qt6/.." "$(cd "${lib_dir}/../.." && pwd)"; do
        if [[ -d "${candidate}/plugins" ]]; then
          echo "${candidate}"
          return 0
        fi
      done
      # Caso Debian multiarch: plugins em <prefix>/lib/<multiarch>/qt6/plugins
      if [[ -d "${lib_dir}/qt6/plugins" ]]; then
        echo "${lib_dir}/qt6"
        return 0
      fi
    fi
  fi

  # Caminho 3: qmake6 (menos confiavel - pode ser de outra Qt no PATH).
  local qmake_bin
  for qmake_bin in qmake6 qmake; do
    if command -v "${qmake_bin}" >/dev/null 2>&1; then
      local prefix
      prefix="$("${qmake_bin}" -query QT_INSTALL_PREFIX 2>/dev/null | head -n1)"
      if [[ -n "${prefix}" && -d "${prefix}" ]]; then
        echo "${prefix}"
        return 0
      fi
    fi
  done

  return 1
}

# Copia plugins Qt (platforms, imageformats, iconengines, styles, wayland*) e a
# arvore de imports QML para <bundle>/plugins e <bundle>/qml, de forma que o
# app rode em maquina sem Qt6 sistema. Ecoa o prefix usado para stdout.
package_copy_qt_resources() {
  local binary="$1"
  local bundle_dir="$2"
  local qt_prefix

  qt_prefix="$(package_resolve_qt_prefix "${binary}")" || {
    echo "package_copy_qt_resources: nao foi possivel resolver o prefix Qt; pulando deploy de plugins/QML." >&2
    return 0
  }

  local plugins_src="${qt_prefix}/plugins"
  local qml_src="${qt_prefix}/qml"
  local plugins_dst="${bundle_dir}/plugins"
  local qml_dst="${bundle_dir}/qml"

  if [[ -d "${plugins_src}" ]]; then
    mkdir -p "${plugins_dst}"
    (
      shopt -s nullglob
      for sub in platforms imageformats iconengines styles wayland-decoration wayland-graphics-integration-client wayland-graphics-integration-server wayland-shell-integration platforminputcontexts; do
        if [[ -d "${plugins_src}/${sub}" ]]; then
          local -a plugin_files=("${plugins_src}/${sub}/"*)
          if [[ "${#plugin_files[@]}" -eq 0 ]]; then
            continue
          fi
          mkdir -p "${plugins_dst}/${sub}"
          cp -fL "${plugin_files[@]}" "${plugins_dst}/${sub}/"
        fi
      done
    )
    # Plugins dependem de libs Qt (libQt6Gui, libQt6DBus, libQt6Wayland*, etc).
    # Copiar as deps transitivas de cada .so de plugin para o lib/ do bundle.
    local plugin_so
    while IFS= read -r -d '' plugin_so; do
      package_copy_runtime_libraries "${plugin_so}" "${bundle_dir}/lib"
    done < <(find "${plugins_dst}" -type f -name '*.so' -print0 2>/dev/null)
  else
    echo "package_copy_qt_resources: ${plugins_src} nao encontrado; plugins nao copiados." >&2
  fi

  if [[ -d "${qml_src}" ]]; then
    mkdir -p "${qml_dst}"
    # Em Qt6 os modulos ficam aninhados (QtQuick/Controls, QtQuick/Layouts,
    # QtQuick/Window, QtQuick/Dialogs). Copiar QtQuick e QtQml recursivamente
    # cobre todos os imports que o app usa. -L resolve symlinks.
    for mod in Qt QtQml QtQuick; do
      if [[ -d "${qml_src}/${mod}" ]]; then
        cp -RL "${qml_src}/${mod}" "${qml_dst}/"
      fi
    done
  else
    echo "package_copy_qt_resources: ${qml_src} nao encontrado; imports QML nao copiados." >&2
  fi

  echo "${qt_prefix}"
}
