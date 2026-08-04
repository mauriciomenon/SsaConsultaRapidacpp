#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
# shellcheck disable=SC1091
source "$repo_root/scripts/lib/native_host_guard.sh"
ssa_native_guard_repo "$repo_root"
[[ "$TMPDIR" == /tmp && "$TMP" == /tmp && "$TEMP" == /tmp ]] || {
  printf 'native guard did not isolate WSL temporary paths\n' >&2
  exit 1
}
ssa_native_guard_tools bash chmod ln mkdir mktemp rm

fixture_root="$(mktemp -d)"
trap 'rm -rf -- "$fixture_root"' EXIT

expect_blocked() {
  if "$@" >/dev/null 2>&1; then
    printf 'expected native guard rejection: %s\n' "$*" >&2
    exit 1
  fi
}

mkdir -p "$fixture_root/repo" "$fixture_root/fake-bin"
expect_blocked ssa_native_guard_repo "$fixture_root/repo"

printf 'MZfake executable\n' > "$fixture_root/fake-bin/cmake"
chmod +x "$fixture_root/fake-bin/cmake"
PATH="$fixture_root/fake-bin:$PATH"
expect_blocked ssa_native_guard_tool cmake

ln -s /mnt/c/Windows/System32/cmd.exe "$fixture_root/fake-bin/glab"
expect_blocked ssa_native_guard_tool glab
expect_blocked ssa_native_guard_path /mnt/c/Users/example/output "$repo_root"

printf '#!/mnt/c/Windows/System32/cmd.exe\n' > "$fixture_root/fake-bin/ctest"
chmod +x "$fixture_root/fake-bin/ctest"
expect_blocked ssa_native_guard_tool ctest

for data_entrypoint in \
  scripts/run-debian.sh \
  scripts/run-debian-smoke-clean.sh \
  scripts/run-debian-smoke-no-clean.sh; do
  if ! grep -Fq '.ssaconsultarapida/data/ssas.db' "$repo_root/$data_entrypoint"; then
    printf 'entrypoint does not use the canonical operational database: %s\n' \
      "$data_entrypoint" >&2
    exit 1
  fi
  if grep -Fq '${repo_root}/data/ssas.db' "$repo_root/$data_entrypoint"; then
    printf 'entrypoint still defaults to a repository database: %s\n' \
      "$data_entrypoint" >&2
    exit 1
  fi
done

printf 'native host guard tests: OK\n'
