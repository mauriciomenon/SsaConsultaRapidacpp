#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
probe_dir="$(mktemp -d)"
trap 'rm -rf "${probe_dir}"' EXIT

entrypoints=(
  build-debian.sh
  build-debian-no-clean.sh
  run-debian.sh
  run-debian-smoke-clean
  run-debian-smoke-no-clean
  package-debian.sh
  build-macos.sh
  build-macos-no-clean.sh
  run-macos.sh
  run-macos-smoke-clean
  run-macos-smoke-no-clean
  package-macos.sh
)

for entrypoint in "${entrypoints[@]}"; do
  path="${repo_root}/${entrypoint}"
  tracked_mode="$(git -C "${repo_root}" ls-files -s -- "${entrypoint}" | awk '{print $1}')"
  if [[ "${tracked_mode}" != "100755" ]]; then
    echo "Entrypoint must be tracked as an executable file: ${entrypoint} (mode ${tracked_mode:-missing})" >&2
    exit 1
  fi
  if [[ -L "${path}" ]]; then
    echo "Entrypoint must be a regular wrapper, not a symlink: ${path}" >&2
    exit 1
  fi
  if [[ ! -x "${path}" ]]; then
    echo "Executable entrypoint not found: ${path}" >&2
    exit 1
  fi
  if [[ "$(head -n 1 "${path}")" != "#!/usr/bin/env bash" ]]; then
    echo "Entrypoint is not a portable Bash wrapper: ${path}" >&2
    exit 1
  fi
  (cd "${probe_dir}" && "${path}" --help >/dev/null)
done

# shellcheck disable=SC1091
source "${repo_root}/scripts/smoke-debian-core.sh"
debian_default_output="$(
  run_debian_smoke_core \
    "${probe_dir}" "${probe_dir}/data/ssas.db" false dev false false 2>&1 || true
)"
if [[ "${debian_default_output}" == *"Database file not found:"* ]]; then
  echo "Debian smoke must allow the missing default database on first run." >&2
  exit 1
fi
debian_explicit_output="$(
  run_debian_smoke_core \
    "${probe_dir}" "${probe_dir}/missing.db" false dev false true 2>&1 || true
)"
if [[ "${debian_explicit_output}" != *"Database file not found:"* ]]; then
  echo "Debian smoke must reject an explicit missing database before build." >&2
  exit 1
fi

# shellcheck disable=SC1091
source "${repo_root}/scripts/smoke-macos-core.sh"
mkdir -p "${probe_dir}/config"
printf '{}\n' >"${probe_dir}/config/preferences.json"
macos_default_output="$(
  run_macos_smoke_core \
    "${probe_dir}" dev "${probe_dir}/data/ssas.db" "${probe_dir}/runtime" \
    "${probe_dir}/runtime/config" "${probe_dir}/runtime/main.png" false \
    "${probe_dir}/config/preferences.json" screenshot true 2>&1 || true
)"
if [[ "${macos_default_output}" == *"Database file not found:"* ]]; then
  echo "macOS smoke must allow the missing default database on first run." >&2
  exit 1
fi
macos_explicit_output="$(
  run_macos_smoke_core \
    "${probe_dir}" dev "${probe_dir}/missing.db" "${probe_dir}/runtime" \
    "${probe_dir}/runtime/config" "${probe_dir}/runtime/main.png" false \
    "${probe_dir}/config/preferences.json" screenshot false 2>&1 || true
)"
if [[ "${macos_explicit_output}" != *"Database file not found:"* ]]; then
  echo "macOS smoke must reject an explicit missing database before build." >&2
  exit 1
fi

echo "Bash entrypoint parity: PASS"
