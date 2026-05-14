#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck disable=SC1091
source "${script_dir}/qt-detect.conf"

require_command() {
  local name="$1"
  local install_hint="$2"
  if ! command -v "$name" >/dev/null 2>&1; then
    printf 'Missing command: %s\n%s\n' "$name" "$install_hint" >&2
    exit 1
  fi
}

is_qt_prefix() {
  local candidate="$1"
  [[ -f "${candidate}/lib/cmake/Qt6/Qt6Config.cmake" ]]
}

normalize_path() {
  local value="$1"
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  value="${value#\"}"
  value="${value%\"}"
  value="${value%\'}"
  value="${value#\'}"
  value="${value%/}"
  value="${value%\\}"
  printf '%s\n' "${value}"
}

split_path_list() {
  local value="$1"
  local sep="$2"
  if [[ -z "${value}" ]]; then
    return
  fi
  IFS="${sep}" read -r -a paths <<< "${value}"
  for candidate in "${paths[@]}"; do
    [[ -n "${candidate}" ]] && normalize_path "${candidate}" && printf '%s\n' "$(normalize_path "${candidate}")"
  done
}

detect_qt_dir() {
  if [[ -n "${QT_DIR:-}" ]] && is_qt_prefix "${QT_DIR}"; then
    printf '%s\n' "${QT_DIR}"
    return 0
  fi

  if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    while IFS= read -r candidate; do
      if is_qt_prefix "${candidate}"; then
        printf '%s\n' "${candidate}"
        return 0
      fi
    done < <(split_path_list "${CMAKE_PREFIX_PATH}" ":")
    while IFS= read -r candidate; do
      if is_qt_prefix "${candidate}"; then
        printf '%s\n' "${candidate}"
        return 0
      fi
    done < <(split_path_list "${CMAKE_PREFIX_PATH}" ";")
    fi

  if command -v brew >/dev/null 2>&1; then
    local brew_qt
    brew_qt="$(brew --prefix qt 2>/dev/null || true)"
    if [[ -n "${brew_qt}" ]] && is_qt_prefix "${brew_qt}"; then
      printf '%s\n' "${brew_qt}"
      return 0
    fi
  fi

  for candidate in \
    "${MACOS_HOMEBREW_ARM_QT}" \
    "${MACOS_HOMEBREW_INTEL_QT}" \
    "${MACOS_QT_ONLINE_INSTALLER_DIR}"; do
    if is_qt_prefix "${candidate}"; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  for candidate in "${HOME}"/Qt/*/macos; do
    if is_qt_prefix "${candidate}"; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  IFS=':' read -r -a linux_qt_dirs <<< "${LINUX_QT_CMAKE_DIRS}"
  for candidate in "${linux_qt_dirs[@]}"; do
    if [[ -f "${candidate}/Qt6Config.cmake" ]]; then
      printf '%s\n' "${candidate%/lib/cmake/Qt6}"
      return 0
    fi
  done

  return 1
}

print_qt_hint() {
  case "$(uname -s)" in
    Darwin)
      cat >&2 <<'EOF'
Qt was not detected.
Install with:
  brew install qt cmake ninja sqlite
Or run with:
  QT_DIR=${MACOS_HOMEBREW_ARM_QT} ./tools/configure-dev.sh
EOF
      ;;
    Linux)
      cat >&2 <<'EOF'
Qt was not detected.
On Debian/Ubuntu install:
  sudo apt update
  sudo apt install -y build-essential cmake ninja-build libsqlite3-dev qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-tools-dev-tools qml6-module-qtquick qml6-module-qtquick-controls
If Qt is installed in a custom path, run:
  QT_DIR=/path/to/qt ./tools/configure-dev.sh
EOF
      ;;
    *)
      cat >&2 <<'EOF'
Qt was not detected.
Set QT_DIR or CMAKE_PREFIX_PATH to the Qt 6 installation prefix.
EOF
      ;;
  esac
}

printf 'Target Qt version: %s\n' "${QT_VERSION}"

require_command cmake "Install CMake 3.24 or newer."
require_command ninja "Install Ninja."

if ! command -v c++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1; then
  printf 'Missing C++ compiler. Install Xcode Command Line Tools on macOS or build-essential on Debian.\n' >&2
  exit 1
fi

if qt_dir="$(detect_qt_dir)"; then
  printf 'Using Qt prefix: %s\n' "${qt_dir}"
  cmake --preset "${preset}" -DCMAKE_PREFIX_PATH="${qt_dir}"
else
  print_qt_hint
  printf 'Trying CMake without CMAKE_PREFIX_PATH in case Qt is registered system-wide.\n' >&2
  cmake --preset "${preset}"
fi
