#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
configure_script="${repo_root}/tools/configure-dev.sh"
fixture_root="$(mktemp -d)"
trap 'rm -rf "${fixture_root}"' EXIT

create_qt_prefix() {
  local version="$1"
  local subdir="$2"
  local prefix="${fixture_root}/Qt/${version}/${subdir}"
  mkdir -p "${prefix}/lib/cmake/Qt6"
  printf '%s\n' '# fake Qt config' >"${prefix}/lib/cmake/Qt6/Qt6Config.cmake"
  printf 'set(PACKAGE_VERSION "%s")\n' "${version}" \
    >"${prefix}/lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake"
}

fail() {
  printf 'FAIL: %s\n' "$1" >&2
  exit 1
}

create_qt_prefix 6.11.0 macos
create_qt_prefix 6.11.1 macos
create_qt_prefix 6.12.0 macos

selected="$(
  HOME="${fixture_root}" \
    QT_INSTALL_ROOT="${fixture_root}/Qt" \
    QT_DIR='' \
    Qt6_DIR='' \
    CMAKE_PREFIX_PATH='' \
    "${configure_script}" --print-qt-prefix
)"
[[ "${selected}" == "${fixture_root}/Qt/6.11.1/macos" ]] || \
  fail "expected latest Qt 6.11.x, got ${selected}"

rm -f "${fixture_root}/Qt/6.11.1/macos/lib/cmake/Qt6/Qt6Config.cmake"
selected="$(
  HOME="${fixture_root}" \
    QT_INSTALL_ROOT="${fixture_root}/Qt" \
    QT_DIR='' \
    Qt6_DIR='' \
    CMAKE_PREFIX_PATH='' \
    "${configure_script}" --print-qt-prefix
)"
[[ "${selected}" == "${fixture_root}/Qt/6.11.0/macos" ]] || \
  fail "expected fallback to Qt 6.11.0, got ${selected}"

mock_bin="${fixture_root}/bin"
cmake_args="${fixture_root}/cmake-args.txt"
mkdir -p "${mock_bin}"
for command_name in ninja c++; do
  printf '%s\n' '#!/usr/bin/env bash' 'exit 0' >"${mock_bin}/${command_name}"
  chmod +x "${mock_bin}/${command_name}"
done
cat >"${mock_bin}/cmake" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$@" >"${QT_TEST_CMAKE_ARGS:?}"
EOF
chmod +x "${mock_bin}/cmake"

HOME="${fixture_root}" \
  QT_INSTALL_ROOT="${fixture_root}/Qt" \
  QT_DIR='' \
  Qt6_DIR='' \
  CMAKE_PREFIX_PATH='' \
  QT_TEST_CMAKE_ARGS="${cmake_args}" \
  PATH="${mock_bin}:/usr/bin:/bin" \
  "${configure_script}" dev >/dev/null 2>&1
grep -Fxq -- "-DQt6_DIR=${selected}/lib/cmake/Qt6" "${cmake_args}" || \
  fail "configure must replace a cached Qt6_DIR with the selected prefix"
grep -Fxq -- '-UQt6*_DIR' "${cmake_args}" || \
  fail "configure must invalidate cached Qt component directories"
grep -Fxq -- '-U*DEPLOYQT_EXECUTABLE' "${cmake_args}" || \
  fail "configure must invalidate cached Qt deployment tools"

if HOME="${fixture_root}" \
  QT_INSTALL_ROOT="${fixture_root}/Qt" \
  QT_DIR="${fixture_root}/Qt/6.12.0/macos" \
  Qt6_DIR='' \
  CMAKE_PREFIX_PATH='' \
  "${configure_script}" --print-qt-prefix >"${fixture_root}/stdout" 2>"${fixture_root}/stderr"; then
  fail "explicit Qt 6.12.0 must be rejected by the 6.11 family contract"
fi
grep -q '6\.11' "${fixture_root}/stderr" || \
  fail "family rejection must explain the required Qt 6.11 family"

printf 'PASS: POSIX Qt family detection\n'
