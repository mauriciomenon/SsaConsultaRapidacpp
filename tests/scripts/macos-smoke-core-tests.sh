#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fixture_root="$(mktemp -d)"
trap 'rm -rf "${fixture_root}"' EXIT

# shellcheck disable=SC1091
source "${repo_root}/scripts/smoke-macos-core.sh"

prepare_exit_code=0
prepare_macos_build_dir() { return "${prepare_exit_code}"; }
cmake_calls=0
ctest_calls=0
app_calls=0
calls_log="${fixture_root}/calls.log"
cmake() {
  cmake_calls=$((cmake_calls + 1))
  printf 'cmake %s\n' "$*" >>"${calls_log}"
}
ctest() {
  ctest_calls=$((ctest_calls + 1))
  printf 'ctest %s\n' "$*" >>"${calls_log}"
}

fail() {
  printf 'FAIL: %s\n' "$1" >&2
  exit 1
}

project_root="${fixture_root}/project"
runtime_dir="${fixture_root}/runtime"
config_dir="${runtime_dir}/config"
db_path="${fixture_root}/ssas.db"
preferences_src="${fixture_root}/preferences.json"
screenshot="${runtime_dir}/main.png"
mkdir -p "${project_root}" "${runtime_dir}"
printf 'db\n' >"${db_path}"
printf '{}\n' >"${preferences_src}"

launches=()
run_macos_app() {
  app_calls=$((app_calls + 1))
  launches+=("${6}")
  if [[ "${6}" == "screenshot" && "${produce_screenshot}" == "true" ]]; then
    printf 'png\n' >"${5}"
  fi
  if [[ "${6}" == "screenshot" ]]; then
    return "${screenshot_exit_code}"
  fi
  return "${open_exit_code}"
}

produce_screenshot=false
screenshot_exit_code=0
open_exit_code=0
printf 'stale\n' >"${screenshot}"
prepare_exit_code=9
if run_macos_smoke_core "${project_root}" dev "${db_path}" "${runtime_dir}" \
  "${config_dir}" "${screenshot}" false "${preferences_src}" screenshot; then
  fail "build preparation failure must propagate from the smoke"
fi
[[ ! -e "${screenshot}" ]] || fail "stale screenshot must be removed before build"
[[ "${cmake_calls}" == "0" ]] || fail "build must not start after preparation failure"
[[ "${ctest_calls}" == "0" ]] || fail "tests must not start after preparation failure"
[[ "${app_calls}" == "0" ]] || fail "app must not start after preparation failure"

prepare_exit_code=0
if run_macos_smoke_core "${project_root}" dev "${db_path}" "${runtime_dir}" \
  "${config_dir}" "${screenshot}" false "${fixture_root}/missing.json" screenshot; then
  fail "missing preferences template must fail the smoke"
fi

printf 'stale\n' >"${screenshot}"
if run_macos_smoke_core "${project_root}" dev "${db_path}" "${runtime_dir}" \
  "${config_dir}" "${screenshot}" false "${preferences_src}" screenshot; then
  fail "stale screenshot must not make a smoke pass"
fi
[[ ! -e "${screenshot}" ]] || fail "stale screenshot must be removed before launch"

produce_screenshot=true
launches=()
: >"${calls_log}"
run_macos_smoke_core "${project_root}" dev "${db_path}" "${runtime_dir}" \
  "${config_dir}" "${screenshot}" false "${preferences_src}" screenshot
[[ "${launches[*]}" == "screenshot" ]] || fail "offscreen smoke must launch once"
[[ -s "${screenshot}" ]] || fail "offscreen smoke must produce a screenshot"
expected_calls="cmake --build ${project_root}/build/dev
ctest --test-dir ${project_root}/build/dev --output-on-failure"
[[ "$(cat "${calls_log}")" == "${expected_calls}" ]] ||
  fail "smoke must build and test the configured directory, got: $(cat "${calls_log}")"

launches=()
run_macos_smoke_core "${project_root}" dev "${db_path}" "${runtime_dir}" \
  "${config_dir}" "${screenshot}" false "${preferences_src}" open
[[ "${launches[*]}" == "screenshot open" ]] || \
  fail "visual smoke must validate offscreen before opening the GUI"

screenshot_exit_code=7
if run_macos_smoke_core "${project_root}" dev "${db_path}" "${runtime_dir}" \
  "${config_dir}" "${screenshot}" false "${preferences_src}" screenshot; then
  fail "application failure must propagate from the smoke"
fi

screenshot_exit_code=0
open_exit_code=7
if run_macos_smoke_core "${project_root}" dev "${db_path}" "${runtime_dir}" \
  "${config_dir}" "${screenshot}" false "${preferences_src}" open; then
  fail "visible application failure must propagate after a valid preflight"
fi

build_root="${fixture_root}/build-macos"
fake_bin="${fixture_root}/bin"
mkdir -p "${build_root}/tools" "${build_root}/scripts/lazy_scripts" "${fake_bin}"
cp "${repo_root}/scripts/build-macos.sh" "${build_root}/scripts/"
cp "${repo_root}/scripts/lazy_scripts/build-macos.sh" "${build_root}/scripts/lazy_scripts/"
# shellcheck disable=SC2016 # $1 belongs to the generated fake configure script.
printf '#!/usr/bin/env bash\nmkdir -p "%s/build/$1"\n' "${build_root}" \
  >"${build_root}/tools/configure-dev.sh"
printf '#!/usr/bin/env bash\nprintf "%%s\\n" "$*" >>"%s/cmake.log"\n' "${fixture_root}" \
  >"${fake_bin}/cmake"
chmod +x "${build_root}/tools/configure-dev.sh" "${fake_bin}/cmake"
for script in scripts/build-macos.sh scripts/lazy_scripts/build-macos.sh; do
  : >"${fixture_root}/cmake.log"
  PATH="${fake_bin}:${PATH}" bash "${build_root}/${script}" >/dev/null
  [[ "$(cat "${fixture_root}/cmake.log")" == "--build ${build_root}/build/dev" ]] ||
    fail "${script} must build the configured directory, got: $(cat "${fixture_root}/cmake.log")"
done

printf 'PASS: macOS smoke core contract\n'
