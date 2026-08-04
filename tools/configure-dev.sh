#!/usr/bin/env bash
set -euo pipefail

print_qt_prefix=false
check_mode=""
case "${1-}" in
  --print-qt-prefix)
    print_qt_prefix=true
    shift
    ;;
  --check)
    check_mode=development
    shift
    ;;
  --check-package)
    check_mode=package
    shift
    ;;
esac
if [[ $# -gt 1 ]]; then
  printf 'Usage: %s [--print-qt-prefix|--check|--check-package] [preset]\n' "$0" >&2
  exit 2
fi
preset="${1:-dev}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
build_dir="${SSA_BUILD_DIR:-${repo_root}/build/${preset}}"
cache_file="${build_dir}/CMakeCache.txt"
# shellcheck disable=SC1091
source "${repo_root}/scripts/lib/native_host_guard.sh"
ssa_native_guard_repo "$repo_root" || exit 1

# shellcheck disable=SC1091
source "${script_dir}/qt-detect.conf"

require_command() {
  local name="$1"
  local install_hint="$2"
  if ! command -v "$name" >/dev/null 2>&1; then
    printf 'Missing command: %s\n%s\n' "$name" "$install_hint" >&2
    exit 1
  fi
  ssa_native_guard_tool "$name" || exit 1
}

warn_clang_format() {
  if command -v clang-format >/dev/null 2>&1; then
    ssa_native_guard_tool clang-format || return 1
    return 0
  fi

  printf 'clang-format not found in PATH.\n' >&2
  case "$(uname -s)" in
    Darwin)
      printf '%s\n' 'Install with:' >&2
      printf '%s\n' '  brew install llvm' >&2
      # shellcheck disable=SC2016,SC2059
      printf '%s\n' "  echo 'export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\"' >> ~/.zshrc" >&2
      # shellcheck disable=SC2016,SC2059
      printf '%s\n' "  echo 'export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\"' >> ~/.bashrc" >&2
      printf '%s\n' '  source ~/.zshrc ~/.bashrc' >&2
      ;;
    Linux)
      printf '%s\n' 'Install with:' >&2
      printf '%s\n' '  sudo apt install -y clang-format' >&2
      ;;
    *)
      printf '%s\n' 'Install with:' >&2
      printf '%s\n' '  winget install --id LLVM.LLVM' >&2
      printf '%s\n' '  Add C:\Program Files\LLVM\bin to the current and user PATH.' >&2
      ;;
  esac
}

is_qt_prefix() {
  local candidate="$1"
  [[ -f "${candidate}/lib/cmake/Qt6/Qt6Config.cmake" ]]
}

qt_version_for_prefix() {
  local candidate="$1"
  local version_file="${candidate}/lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake"
  [[ -f "${version_file}" ]] || return 1
  sed -n 's/^[[:space:]]*set(PACKAGE_VERSION[[:space:]]*"\([0-9][0-9.]*\)").*/\1/p' \
    "${version_file}" | head -n 1
}

qt_version_matches_family() {
  local version="$1"
  [[ "${version}" == "${QT_VERSION_FAMILY}" || "${version}" == "${QT_VERSION_FAMILY}."* ]]
}

qt_prefix_from_cmake_dir() {
  local candidate="$1"
  if [[ "${candidate}" == */lib/cmake/Qt6 ]]; then
    candidate="${candidate%/lib/cmake/Qt6}"
  fi
  normalize_path "${candidate}"
}

validate_explicit_qt_prefix() {
  local source_name="$1"
  local candidate="$2"
  local version
  candidate="$(qt_prefix_from_cmake_dir "${candidate}")"
  if ! is_qt_prefix "${candidate}"; then
    printf '%s does not point to a Qt prefix: %s\n' "${source_name}" "${candidate}" >&2
    return 1
  fi
  version="$(qt_version_for_prefix "${candidate}" || true)"
  if [[ -z "${version}" ]] || ! qt_version_matches_family "${version}"; then
    printf '%s must point to Qt %s.x; detected version: %s\n' \
      "${source_name}" "${QT_VERSION_FAMILY}" "${version:-unknown}" >&2
    return 1
  fi
  printf '%s\n' "${candidate}"
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
  if [[ -z "${value}" ]]; then
    return
  fi
  printf '%s\n' "${value}"
}

detect_explicit_qt_dir() {
  if [[ -n "${QT_DIR:-}" ]]; then
    validate_explicit_qt_prefix QT_DIR "${QT_DIR}"
    return
  fi

  if [[ -n "${Qt6_DIR:-}" ]]; then
    validate_explicit_qt_prefix Qt6_DIR "${Qt6_DIR}"
    return
  fi

  if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
    while IFS= read -r candidate; do
      candidate="$(qt_prefix_from_cmake_dir "${candidate}")"
      if is_qt_prefix "${candidate}"; then
        validate_explicit_qt_prefix CMAKE_PREFIX_PATH "${candidate}"
        return
      fi
    done < <(printf '%s\n' "${CMAKE_PREFIX_PATH}" | tr ':;' '\n')
  fi

  return 2
}

version_is_newer() {
  local candidate="$1"
  local current="$2"
  local candidate_major candidate_minor candidate_patch
  local current_major current_minor current_patch
  IFS=. read -r candidate_major candidate_minor candidate_patch _ <<<"${candidate}"
  IFS=. read -r current_major current_minor current_patch _ <<<"${current}"
  candidate_patch="${candidate_patch:-0}"
  current_patch="${current_patch:-0}"
  ((candidate_major > current_major)) ||
    ((candidate_major == current_major && candidate_minor > current_minor)) ||
    ((candidate_major == current_major && candidate_minor == current_minor && candidate_patch > current_patch))
}

detect_auto_qt_dir() {
  local best_path=""
  local best_version="0.0.0"
  local candidate version linux_prefix
  local -a candidates=()

  if [[ -n "${QT_INSTALL_ROOT:-}" ]]; then
    for candidate in "${QT_INSTALL_ROOT}"/*/macos "${QT_INSTALL_ROOT}"/*/gcc_64; do
      [[ -d "${candidate}" ]] && candidates+=("${candidate}")
    done
  else
    candidates+=(
      "${MACOS_QT_ONLINE_INSTALLER_DIR}"
      "${MACOS_HOMEBREW_ARM_QT}"
      "${MACOS_HOMEBREW_INTEL_QT}"
    )

    for candidate in "${HOME}"/Qt/*/macos "${HOME}"/Qt/*/gcc_64; do
      [[ -d "${candidate}" ]] && candidates+=("${candidate}")
    done

    if command -v brew >/dev/null 2>&1; then
      candidate="$(brew --prefix qt 2>/dev/null || true)"
      [[ -n "${candidate}" ]] && candidates+=("${candidate}")
    fi

    IFS=':' read -r -a linux_qt_dirs <<<"${LINUX_QT_CMAKE_DIRS}"
    for candidate in "${linux_qt_dirs[@]}"; do
      linux_prefix="${candidate%/lib/cmake/Qt6}"
      candidates+=("${linux_prefix}")
    done
  fi

  for candidate in "${candidates[@]}"; do
    is_qt_prefix "${candidate}" || continue
    version="$(qt_version_for_prefix "${candidate}" || true)"
    if [[ -z "${version}" ]] || ! qt_version_matches_family "${version}"; then
      continue
    fi
    if [[ -z "${best_path}" ]] || version_is_newer "${version}" "${best_version}"; then
      best_path="${candidate}"
      best_version="${version}"
    fi
  done

  [[ -n "${best_path}" ]] || return 1
  printf '%s\n' "${best_path}"
}

detect_qt_dir() {
  local explicit_path explicit_status
  if explicit_path="$(detect_explicit_qt_dir)"; then
    printf '%s\n' "${explicit_path}"
    return 0
  else
    explicit_status=$?
  fi
  if [[ ${explicit_status} -eq 1 ]]; then
    return 1
  fi

  detect_auto_qt_dir
}

print_compiler_choices() {
  local compiler
  local -a compilers=()
  for compiler in clang++ g++ c++; do
    if command -v "${compiler}" >/dev/null 2>&1; then
      ssa_native_guard_tool "${compiler}" || return 1
      compilers+=("$(command -v "${compiler}")")
    fi
  done
  if [[ ${#compilers[@]} -gt 1 ]]; then
    printf 'Detected C++ compilers: %s\n' "${compilers[*]}"
    printf 'Select explicitly with CC=/path/to/cc CXX=/path/to/c++.\n'
  fi
}

print_qt_hint() {
  case "$(uname -s)" in
    Darwin)
      cat >&2 <<'EOF'
Qt 6.11.x was not detected.
Install with:
  brew install qt cmake ninja sqlite
Or run with:
  QT_DIR=${MACOS_HOMEBREW_ARM_QT} ./tools/configure-dev.sh
EOF
      ;;
    Linux)
      cat >&2 <<'EOF'
Qt 6.11.x was not detected.
On Debian/Ubuntu install:
  sudo apt update
  sudo apt install -y build-essential cmake ninja-build libsqlite3-dev qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-tools-dev-tools qml6-module-qtquick qml6-module-qtquick-controls
If Qt is installed in a custom path, run:
  QT_DIR=/path/to/qt ./tools/configure-dev.sh
EOF
      ;;
    *)
      cat >&2 <<'EOF'
Qt 6.11.x was not detected.
Set QT_DIR or CMAKE_PREFIX_PATH to the Qt 6 installation prefix.
EOF
      ;;
  esac
}

command_version() {
  local command_path="$1"
  local output version
  output="$("${command_path}" --version 2>/dev/null | head -n 1 || true)"
  version="$(grep -Eo '[0-9]+([.][0-9]+){1,2}' <<<"${output}" | head -n 1 || true)"
  printf '%s\n' "${version:-unknown}"
}

print_check_record() {
  printf '%s %s path=%s version=%s\n' "$1" "$2" "$3" "$4"
}

print_linux_hint() {
  if command -v pacman >/dev/null 2>&1; then
    printf '%s\n' 'HINT dependencies sudo pacman -S --needed base-devel cmake ninja pkgconf sqlite'
  else
    printf '%s\n' 'HINT dependencies sudo apt install -y build-essential cmake ninja-build pkg-config libsqlite3-dev'
  fi
}

print_check_hint() {
  local dependency="$1"
  case "$(uname -s)" in
    Darwin)
      printf 'HINT %s brew install qt cmake ninja sqlite\n' "${dependency}"
      ;;
    Linux)
      print_linux_hint
      if [[ "${dependency}" == "qt" ]]; then
        printf '%s\n' 'HINT qt Qt 6.11.x: https://www.qt.io/download-qt-installer-oss'
      fi
      ;;
    *)
      printf 'HINT %s install the required tool for this platform\n' "${dependency}"
      ;;
  esac
}

check_command_dependency() {
  local dependency="$1"
  local command_name="$2"
  local command_path
  if command_path="$(command -v "${command_name}" 2>/dev/null)"; then
    if ! ssa_native_guard_tool "${command_path}"; then
      return 1
    fi
    print_check_record OK "${dependency}" "${command_path}" "$(command_version "${command_path}")"
    return 0
  fi
  print_check_record MISSING "${dependency}" - -
  print_check_hint "${dependency}"
  return 1
}

check_qt_dependency() {
  local candidate="" version=""
  if [[ -n "${QT_DIR:-}" ]]; then
    candidate="$(qt_prefix_from_cmake_dir "${QT_DIR}")"
  elif [[ -n "${Qt6_DIR:-}" ]]; then
    candidate="$(qt_prefix_from_cmake_dir "${Qt6_DIR}")"
  fi

  if [[ -n "${candidate}" ]]; then
    if ! is_qt_prefix "${candidate}"; then
      print_check_record MISSING qt "${candidate}" -
      print_check_hint qt
      return 1
    fi
    version="$(qt_version_for_prefix "${candidate}" || true)"
    if [[ -z "${version}" ]] || ! qt_version_matches_family "${version}"; then
      print_check_record UNSUPPORTED qt "${candidate}" "${version:-unknown}"
      print_check_hint qt
      return 1
    fi
  elif candidate="$(detect_qt_dir 2>/dev/null)"; then
    version="$(qt_version_for_prefix "${candidate}" || true)"
  else
    print_check_record MISSING qt - -
    print_check_hint qt
    return 1
  fi

  checked_qt_dir="${candidate}"
  print_check_record OK qt "${candidate}" "${version}"
}

check_sqlite_dependency() {
  local version path
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sqlite3; then
    version="$(pkg-config --modversion sqlite3)"
    path="$(pkg-config --variable=includedir sqlite3)"
    print_check_record OK sqlite "${path}" "${version}"
    return 0
  fi
  print_check_record MISSING sqlite - -
  print_check_hint sqlite
  return 1
}

run_dependency_check() {
  local failed=0 compiler=""
  checked_qt_dir=""
  check_qt_dependency || failed=1
  check_command_dependency cmake cmake || failed=1
  check_command_dependency ninja ninja || failed=1
  for compiler in "${CXX:-}" c++ clang++ g++; do
    [[ -n "${compiler}" ]] || continue
    if command -v "${compiler}" >/dev/null 2>&1; then
      break
    fi
    compiler=""
  done
  if [[ -n "${compiler}" ]]; then
    compiler="$(command -v "${compiler}")"
    if ssa_native_guard_tool "${compiler}"; then
      print_check_record OK compiler "${compiler}" "$(command_version "${compiler}")"
    else
      failed=1
    fi
  else
    print_check_record MISSING compiler - -
    print_check_hint compiler
    failed=1
  fi
  check_sqlite_dependency || failed=1

  if [[ "${check_mode}" == "package" ]]; then
    case "$(uname -s)" in
      Darwin)
        if [[ -x "${checked_qt_dir}/bin/macdeployqt" ]]; then
          print_check_record OK package-macdeployqt "${checked_qt_dir}/bin/macdeployqt" "$(command_version "${checked_qt_dir}/bin/macdeployqt")"
        else
          print_check_record MISSING package-macdeployqt - -
          print_check_hint package-macdeployqt
          failed=1
        fi
        check_command_dependency package-ditto ditto || failed=1
        check_command_dependency package-lipo lipo || failed=1
        check_command_dependency package-hdiutil hdiutil || failed=1
        check_command_dependency package-codesign codesign || failed=1
        ;;
      Linux)
        check_command_dependency package-tar tar || failed=1
        check_command_dependency package-file file || failed=1
        if command -v pacman >/dev/null 2>&1; then
          check_command_dependency package-makepkg makepkg || failed=1
        else
          check_command_dependency package-dpkg dpkg-deb || failed=1
          check_command_dependency package-fakeroot fakeroot || failed=1
        fi
        ;;
      *)
        print_check_record UNSUPPORTED package-platform "$(uname -s)" -
        failed=1
        ;;
    esac
  fi
  return "${failed}"
}

if [[ -z "${QT_VERSION:-}" || -z "${QT_VERSION_FAMILY:-}" ]]; then
  printf 'Missing required variable: QT_VERSION or QT_VERSION_FAMILY\n' >&2
  exit 1
fi

if [[ -n "${check_mode}" ]]; then
  run_dependency_check
  exit $?
fi

qt_dir=""
if ! qt_dir="$(detect_qt_dir)"; then
  print_qt_hint
  exit 1
fi

if [[ "${print_qt_prefix}" == "true" ]]; then
  printf '%s\n' "${qt_dir}"
  exit 0
fi

printf 'Target Qt family: %s.x (reference patch: %s)\n' "${QT_VERSION_FAMILY}" "${QT_VERSION}"
printf 'Using Qt version: %s\n' "$(qt_version_for_prefix "${qt_dir}")"
printf 'Using Qt prefix: %s\n' "${qt_dir}"

require_command cmake "Install CMake 3.24 or newer."
require_command ninja "Install Ninja."

if ! command -v c++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1; then
  printf 'Missing C++ compiler. Install Xcode Command Line Tools on macOS or build-essential on Debian.\n' >&2
  exit 1
fi

print_compiler_choices || exit 1
cmake_fresh_args=()
if [[ -f "${cache_file}" ]]; then
  cached_build_dir="$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' "${cache_file}" | head -n 1)"
  cached_generator="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "${cache_file}" | head -n 1)"
  cached_make_program="$(sed -n 's/^CMAKE_MAKE_PROGRAM:FILEPATH=//p' "${cache_file}" | head -n 1)"
  cached_compiler="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "${cache_file}" | head -n 1)"
  if [[ -n "${cached_make_program}" && -x "${cached_make_program}" ]]; then
    ssa_native_guard_tool "${cached_make_program}" || exit 1
  fi
  if [[ -n "${cached_compiler}" && -x "${cached_compiler}" ]]; then
    ssa_native_guard_tool "${cached_compiler}" || exit 1
  fi
  if [[ "${cached_build_dir}" != "${build_dir}" || "${cached_generator}" != "Ninja" ||
    ! -x "${cached_make_program}" ||
    (-n "${cached_compiler}" && ! -x "${cached_compiler}") ]]; then
    printf 'Refreshing foreign or stale CMake cache: %s\n' "${cache_file}"
    cmake_fresh_args+=(--fresh)
  fi
fi
(cd "${repo_root}" && cmake --preset "${preset}" -B "${build_dir}" \
  "${cmake_fresh_args[@]}" \
  '-UQt6*_DIR' \
  '-U*DEPLOYQT_EXECUTABLE' \
  -DCMAKE_PREFIX_PATH="${qt_dir}" \
  -DQt6_DIR="${qt_dir}/lib/cmake/Qt6")

warn_clang_format || exit 1
