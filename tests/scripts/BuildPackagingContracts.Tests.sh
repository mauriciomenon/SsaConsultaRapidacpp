#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "${repo_root}/build"
test_root="$(mktemp -d "${repo_root}/build/ssa-build-contracts.XXXXXX")"
trap 'rm -rf "${test_root}"' EXIT

fixture_repo="${test_root}/gitlab_repo/ssa_consulta_rapida_cpp"
fake_bin="${test_root}/bin"
mkdir -p "${fixture_repo}/scripts/lazy_scripts" "${fixture_repo}/scripts/lib" "${fixture_repo}/tools" \
  "${fixture_repo}/build/debian/amd64/dev" "${fake_bin}"
cp "${repo_root}/scripts/build-debian.sh" "${fixture_repo}/scripts/"
cp "${repo_root}/scripts/lazy_scripts/build-debian.sh" \
  "${fixture_repo}/scripts/lazy_scripts/"
cp "${repo_root}/scripts/debian-paths.sh" "${fixture_repo}/scripts/"
cp "${repo_root}/scripts/lib/native_host_guard.sh" "${fixture_repo}/scripts/lib/"

printf 'stale\n' > "${fixture_repo}/build/debian/amd64/dev/stale-output.txt"
cat > "${fixture_repo}/tools/configure-dev.sh" <<'EOF_CONFIGURE'
#!/usr/bin/env bash
set -euo pipefail
mkdir -p "${SSA_BUILD_DIR:?SSA_BUILD_DIR is required}"
EOF_CONFIGURE
cat > "${fake_bin}/cmake" <<'EOF_CMAKE'
#!/usr/bin/env bash
if [[ "${PWD}" != "${SSA_EXPECTED_CWD}" ]]; then
  echo "cmake was invoked from ${PWD}, expected ${SSA_EXPECTED_CWD}" >&2
  exit 87
fi
exit 0
EOF_CMAKE
chmod +x "${fixture_repo}/tools/configure-dev.sh" "${fake_bin}/cmake" \
  "${fixture_repo}/scripts/lib/native_host_guard.sh"

HOME="${test_root}" SSA_EXPECTED_CWD="${fixture_repo}" PATH="${fake_bin}:${PATH}" \
  "${fixture_repo}/scripts/build-debian.sh"
if [[ -e "${fixture_repo}/build/debian/amd64/dev/stale-output.txt" ]]; then
  echo "build-debian.sh reused stale output by default." >&2
  exit 1
fi

printf 'keep\n' > "${fixture_repo}/build/debian/amd64/dev/incremental-output.txt"
HOME="${test_root}" SSA_EXPECTED_CWD="${fixture_repo}" PATH="${fake_bin}:${PATH}" \
  "${fixture_repo}/scripts/lazy_scripts/build-debian.sh"
if [[ ! -e "${fixture_repo}/build/debian/amd64/dev/incremental-output.txt" ]]; then
  echo "lazy Debian build did not preserve the explicit incremental cache." >&2
  exit 1
fi

native_arch="$(dpkg --print-architecture)"
if [[ "${native_arch}" == "amd64" ]]; then
  foreign_arch="arm64"
else
  foreign_arch="amd64"
fi
foreign_output="$(
  PATH="${fake_bin}:${PATH}" "${repo_root}/scripts/package-debian.sh" \
    --project-root "${fixture_repo}" --version 1.2.3 \
    --dist-dir "${test_root}/foreign-dist" --arch "${foreign_arch}" \
    --skip-tests 2>&1 || true
)"
if [[ "${foreign_output}" != *"does not match native architecture"* ]]; then
  echo "Debian package accepted or misdiagnosed a foreign architecture." >&2
  exit 1
fi

# shellcheck disable=SC1091
source "${repo_root}/scripts/lib/package_common.sh"
dist_root="${test_root}/dist/linux/amd64/gcc"
stage_root="${test_root}/stage"
mkdir -p "${dist_root}/final" "${stage_root}"
printf 'previous\n' > "${dist_root}/final/previous-release.txt"
printf '{"release":"old"}\n' > "${dist_root}/current.json"
printf 'app\n' > "${stage_root}/ssa_consulta_rapida"

required=(
  "ssa_consulta_rapida"
  "ssa_consulta_rapida.deb"
  "ssa_consulta_rapida.zip"
  "ssa_consulta_rapida-standalone/ssa_consulta_rapida"
)
qt_kit='C:\Qt "fixture"'
if package_publish_release_set "${stage_root}" "${dist_root}" "1.2.3" \
  "abc123" "debian" "amd64" "gcc" "release" "${qt_kit}" "${fixture_repo}" \
  "gcc" "test" "ld" "test" "false" "${required[@]}"; then
  echo "Incomplete release set was accepted." >&2
  exit 1
fi
grep -Fxq "previous" "${dist_root}/final/previous-release.txt"
grep -Fq '"release":"old"' "${dist_root}/current.json"

mkdir -p "${stage_root}/ssa_consulta_rapida-standalone"
printf 'deb\n' > "${stage_root}/ssa_consulta_rapida.deb"
printf 'zip\n' > "${stage_root}/ssa_consulta_rapida.zip"
printf 'standalone\n' \
  > "${stage_root}/ssa_consulta_rapida-standalone/ssa_consulta_rapida"
package_publish_release_set "${stage_root}" "${dist_root}" "1.2.3" \
  "abc123" "debian" "amd64" "gcc" "release" "${qt_kit}" "${fixture_repo}" \
  "gcc" "test" "ld" "test" "false" "${required[@]}"

for required_path in "${required[@]}" release.json SHA256SUMS; do
  if [[ ! -e "${dist_root}/final/${required_path}" ]]; then
    echo "Promoted release is missing: ${required_path}" >&2
    exit 1
  fi
done
grep -Fq '"release": "1.2.3-abc123-debian-amd64-gcc"' "${dist_root}/current.json"
grep -Fq '"toolchain": "gcc"' "${dist_root}/current.json"
grep -Fq '"compiler": "gcc"' "${dist_root}/current.json"
grep -Fq '"qtKit": "C:\\Qt \"fixture\""' "${dist_root}/current.json"
if [[ -e "${dist_root}/.publish.lock" || -e "${stage_root}" ]]; then
  echo "Release publication left transient files." >&2
  exit 1
fi

second_stage="${test_root}/stage-second"
invalid_release="${dist_root}/releases/manual-copy"
mkdir -p "${second_stage}/ssa_consulta_rapida-standalone" "${invalid_release}"
printf 'app2\n' > "${second_stage}/ssa_consulta_rapida"
printf 'deb2\n' > "${second_stage}/ssa_consulta_rapida.deb"
printf 'zip2\n' > "${second_stage}/ssa_consulta_rapida.zip"
printf 'standalone2\n' > "${second_stage}/ssa_consulta_rapida-standalone/ssa_consulta_rapida"
printf '{"release":"manual-copy","platform":"debian"}\n' > "${invalid_release}/release.json"

package_publish_release_set "${second_stage}" "${dist_root}" "1.2.4" \
  "def456" "debian" "amd64" "gcc" "release" "${qt_kit}" "${fixture_repo}" \
  "gcc" "test" "ld" "test" "false" "${required[@]}"

grep -Fq '"release": "1.2.3-abc123-debian-amd64-gcc"' "${dist_root}/previous.json"
grep -Fq '"release": "1.2.4-def456-debian-amd64-gcc"' "${dist_root}/current.json"
[[ -f "${dist_root}/previous/ssa_consulta_rapida" ]]
[[ ! -e "${dist_root}/releases/1.2.3-abc123-debian-amd64-gcc" ]]
[[ -f "${invalid_release}/release.json" ]]

failure_stage="${test_root}/stage-failure"
release_dir="${dist_root}/releases/1.2.4-def456-debian-amd64-gcc"
mkdir -p "${failure_stage}/ssa_consulta_rapida-standalone" "${release_dir}"
printf 'app3\n' > "${failure_stage}/ssa_consulta_rapida"
printf 'deb3\n' > "${failure_stage}/ssa_consulta_rapida.deb"
printf 'zip3\n' > "${failure_stage}/ssa_consulta_rapida.zip"
printf 'standalone3\n' > "${failure_stage}/ssa_consulta_rapida-standalone/ssa_consulta_rapida"
printf 'different-hash\n' > "${release_dir}/SHA256SUMS"
cp "${dist_root}/current.json" "${test_root}/current-before.json"
cp "${dist_root}/previous.json" "${test_root}/previous-before.json"

if package_publish_release_set "${failure_stage}" "${dist_root}" "1.2.4" \
  "def456" "debian" "amd64" "gcc" "release" "${qt_kit}" "${fixture_repo}" \
  "gcc" "test" "ld" "test" "true" "${required[@]}"; then
  echo "Conflicting tagged release was accepted." >&2
  exit 1
fi
cmp -s "${test_root}/current-before.json" "${dist_root}/current.json"
cmp -s "${test_root}/previous-before.json" "${dist_root}/previous.json"
[[ -f "${dist_root}/previous/ssa_consulta_rapida" ]]

integrity_dist="${test_root}/dist-integrity"
integrity_stage="${test_root}/stage-integrity"
integrity_stage_retry="${test_root}/stage-integrity-retry"
for integrity_root in "${integrity_stage}" "${integrity_stage_retry}"; do
  mkdir -p "${integrity_root}/ssa_consulta_rapida-standalone"
  printf 'app\n' > "${integrity_root}/ssa_consulta_rapida"
  printf 'deb\n' > "${integrity_root}/ssa_consulta_rapida.deb"
  printf 'zip\n' > "${integrity_root}/ssa_consulta_rapida.zip"
  printf 'standalone\n' \
    > "${integrity_root}/ssa_consulta_rapida-standalone/ssa_consulta_rapida"
done
package_publish_release_set "${integrity_stage}" "${integrity_dist}" "1.2.5" \
  "fed789" "debian" "amd64" "gcc" "release" "${qt_kit}" "${fixture_repo}" \
  "gcc" "test" "ld" "test" "true" "${required[@]}"
printf 'tampered\n' \
  > "${integrity_dist}/releases/1.2.5-fed789-debian-amd64-gcc/ssa_consulta_rapida"
if package_publish_release_set "${integrity_stage_retry}" "${integrity_dist}" \
  "1.2.5" "fed789" "debian" "amd64" "gcc" "release" "${qt_kit}" \
  "${fixture_repo}" "gcc" "test" "ld" "test" "true" "${required[@]}"; then
  echo "Corrupted immutable Debian release was reused." >&2
  exit 1
fi

echo "Build and packaging contract tests passed."
