include(FetchContent)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Cache fetched dependencies outside the build directory so a clean rebuild
# (e.g. make_clean, smoke-macos --clean) does not force a network re-download.
# This makes the build resilient to offline / DNS failures after the first
# successful fetch. The cache lives under <repo>/.deps-cache and is reused by
# every preset (dev, release, dev-asan, dev-tsan, dev-cov).
set(SSA_FETCHCACHE_DIR
    "${CMAKE_SOURCE_DIR}/.deps-cache"
    CACHE PATH "Persistent fetch cache for third-party deps")
set(FETCHCONTENT_BASE_DIR "${SSA_FETCHCACHE_DIR}")
option(SSA_FETCHCONTENT_UPDATES_DISCONNECTED
       "Reuse cached FetchContent deps without checking remote updates" ON)
set(FETCHCONTENT_UPDATES_DISCONNECTED ${SSA_FETCHCONTENT_UPDATES_DISCONNECTED})

FetchContent_Declare(
  miniz
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG 3.0.2
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(miniz)

if(SSA_BUILD_TESTS)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
