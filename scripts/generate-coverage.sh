#!/usr/bin/env bash
# Generate a source-based coverage HTML report using the dev-cov preset.
#
# Usage:
#   ./scripts/generate-coverage.sh
#
# Requires: dev-cov preset (Apple clang + -fprofile-instr-generate -fcoverage-mapping),
#           llvm-profdata, llvm-cov, genhtml (lcov).
#
# Output: build/dev-cov/coverage_html/index.html

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT_DIR/scripts/lib/native_host_guard.sh"
ssa_native_guard_repo "$ROOT_DIR" || exit 1
ssa_native_guard_tools rm ctest llvm-profdata llvm-cov lcov genhtml || exit 1
BUILD_DIR="${ROOT_DIR}/build/dev-cov"
COV_DIR="${BUILD_DIR}/coverage_html"
PROFRAW_GLOB="/tmp/ssa_cov_*.profraw"
PROFDATA="/tmp/ssa_cov.profdata"
LCOV_RAW="/tmp/ssa_cov.lcov"
LCOV_SRC="/tmp/ssa_cov_src.lcov"

if [ ! -d "${BUILD_DIR}" ]; then
  echo "ERROR: build/dev-cov not found. Run: cmake --preset dev-cov && cmake --build --preset dev-cov" >&2
  exit 1
fi

echo "==> Cleaning previous profraw artifacts"
# shellcheck disable=SC2086 # glob expansion is intentional for profraw cleanup
rm -f ${PROFRAW_GLOB} "${PROFDATA}" "${LCOV_RAW}" "${LCOV_SRC}"

echo "==> Running tests under coverage instrumentation"
# %p expands to the process id so each test binary writes its own profraw.
# Run all test binaries (unit + integration) so coverage reflects the whole
# suite, not just the pure Catch2 unit tests.
export LLVM_PROFILE_FILE="/tmp/ssa_cov_%p.profraw"
ctest --preset dev-cov --output-on-failure
# Also invoke the standalone test binaries directly so their profraw is captured
# even when ctest wraps them in a way that drops the env var.
for bin in ssa_unit_tests ssa_integration_tests; do
  if [ -x "${BUILD_DIR}/${bin}" ]; then
    "${BUILD_DIR}/${bin}" >/dev/null 2>&1 || true
  fi
done

# shellcheck disable=SC2012,SC2086 # glob + ls counting of profraw files is intentional
PROFRAW_COUNT=$(ls ${PROFRAW_GLOB} 2>/dev/null | wc -l | tr -d ' ')
if [ "${PROFRAW_COUNT}" -eq 0 ]; then
  echo "ERROR: no .profraw files generated. Tests may have crashed." >&2
  exit 1
fi
echo "==> Found ${PROFRAW_COUNT} profraw files"

echo "==> Merging profile data"
# shellcheck disable=SC2086 # glob expansion is intentional for profraw merge
llvm-profdata merge -sparse ${PROFRAW_GLOB} -o "${PROFDATA}"

echo "==> Exporting to lcov format (src/ only, excludes catch2 deps)"
# Use ssa_integration_tests as the symbol source: it links domain/query/infra/
# application, giving a broader view than the pure-Catch2 ssa_unit_tests.
llvm-cov export "${BUILD_DIR}/ssa_integration_tests" \
  -instr-profile="${PROFDATA}" \
  -format=lcov \
  src/ > "${LCOV_RAW}" 2>/dev/null || true

# llvm-cov may prepend warnings; strip them and keep only src/ files.
grep -v "^warning:" "${LCOV_RAW}" > "${LCOV_SRC}.tmp"
mv "${LCOV_SRC}.tmp" "${LCOV_RAW}"
lcov --extract "${LCOV_RAW}" "*/src/*" -o "${LCOV_SRC}" \
  --ignore-errors inconsistent,corrupt,unsupported 2>/dev/null || true

echo "==> Generating HTML report"
rm -rf "${COV_DIR}"
genhtml "${LCOV_SRC}" -o "${COV_DIR}" \
  --no-branch-coverage \
  --ignore-errors inconsistent,corrupt,unsupported,source \
  --synthesize-missing 2>/dev/null

echo ""
echo "==> Coverage report ready:"
echo "    ${COV_DIR}/index.html"
echo "    open with: open ${COV_DIR}/index.html"

echo ""
echo "==> Summary:"
llvm-cov report "${BUILD_DIR}/ssa_integration_tests" \
  -instr-profile="${PROFDATA}" \
  src/ 2>/dev/null | tail -5
