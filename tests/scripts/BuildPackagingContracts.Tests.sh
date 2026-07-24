#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/ssa-build-contracts.XXXXXX")"
trap 'rm -rf "${test_root}"' EXIT

fixture_repo="${test_root}/repo"
fake_bin="${test_root}/bin"
mkdir -p "${fixture_repo}/scripts/lazy_scripts" "${fixture_repo}/tools" \
  "${fixture_repo}/build/dev" "${fake_bin}"
cp "${repo_root}/scripts/build-debian.sh" "${fixture_repo}/scripts/"
cp "${repo_root}/scripts/lazy_scripts/build-debian.sh" \
  "${fixture_repo}/scripts/lazy_scripts/"

printf 'stale\n' > "${fixture_repo}/build/dev/stale-output.txt"
cat > "${fixture_repo}/tools/configure-dev.sh" <<'EOF_CONFIGURE'
#!/usr/bin/env bash
set -euo pipefail
mkdir -p "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/build/${1}"
EOF_CONFIGURE
cat > "${fake_bin}/cmake" <<'EOF_CMAKE'
#!/usr/bin/env bash
exit 0
EOF_CMAKE
chmod +x "${fixture_repo}/tools/configure-dev.sh" "${fake_bin}/cmake"

PATH="${fake_bin}:${PATH}" "${fixture_repo}/scripts/build-debian.sh"
if [[ -e "${fixture_repo}/build/dev/stale-output.txt" ]]; then
  echo "build-debian.sh reused stale output by default." >&2
  exit 1
fi

printf 'keep\n' > "${fixture_repo}/build/dev/incremental-output.txt"
PATH="${fake_bin}:${PATH}" "${fixture_repo}/scripts/lazy_scripts/build-debian.sh"
if [[ ! -e "${fixture_repo}/build/dev/incremental-output.txt" ]]; then
  echo "lazy Debian build did not preserve the explicit incremental cache." >&2
  exit 1
fi

# shellcheck disable=SC1091
source "${repo_root}/scripts/lib/package_common.sh"
dist_root="${test_root}/dist/linux/amd64"
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
if package_publish_release_set "${stage_root}" "${dist_root}" "1.2.3" \
  "abc123" "debian" "amd64" "release" "${required[@]}"; then
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
  "abc123" "debian" "amd64" "release" "${required[@]}"

for required_path in "${required[@]}" release.json SHA256SUMS; do
  if [[ ! -e "${dist_root}/final/${required_path}" ]]; then
    echo "Promoted release is missing: ${required_path}" >&2
    exit 1
  fi
done
grep -Fq '"release": "1.2.3-abc123"' "${dist_root}/current.json"
if [[ -e "${dist_root}/.publish.lock" || -e "${stage_root}" ]]; then
  echo "Release publication left transient files." >&2
  exit 1
fi

echo "Build and packaging contract tests passed."
