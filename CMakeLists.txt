cmake_minimum_required(VERSION 3.24)

project(SSAConsultaRapidaCpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(SSA_BUILD_TESTS "Build tests" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Concurrent Core Gui Qml Quick QuickControls2 Sql Test)
find_package(SQLite3 REQUIRED)

include(cmake/Dependencies.cmake)

qt_standard_project_setup(REQUIRES 6.6)
qt_policy(SET QTP0004 NEW)

add_library(ssa_core STATIC
    src/domain/ColumnCatalog.cpp
    src/query/SearchParser.cpp
    src/query/SqlQueryBuilder.cpp
    src/query/SsaQueryService.cpp
)
target_include_directories(ssa_core PUBLIC src)

add_library(ssa_infra STATIC
    src/infra/sqlite/SqliteConnection.cpp
    src/infra/sqlite/SqliteSsaRepository.cpp
    src/infra/sqlite/SqliteSchemaInspector.cpp
    src/infra/preferences/JsonUserPreferencesStore.cpp
)
target_include_directories(ssa_infra PUBLIC src)
target_link_libraries(ssa_infra PUBLIC ssa_core SQLite3::SQLite3 Qt6::Core)

add_library(ssa_presentation STATIC
    src/platform/DesktopExternalCommandPort.cpp
    src/platform/AppPaths.cpp
    src/platform/StartupOptions.cpp
    src/presentation/SsaTableModel.cpp
    src/presentation/SearchViewModel.cpp
    src/presentation/FilterPanelViewModel.cpp
    src/presentation/DetailsViewModel.cpp
    src/presentation/StatusViewModel.cpp
    src/presentation/CommandViewModel.cpp
    src/presentation/MainViewModel.cpp
)
target_include_directories(ssa_presentation PUBLIC src)
target_link_libraries(ssa_presentation PUBLIC
    ssa_core
    ssa_infra
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
)

qt_add_executable(ssa_consulta_rapida
    app/desktop/main.cpp
)

set_source_files_properties(app/desktop/qml/Theme.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(ssa_consulta_rapida
    URI SsaConsultaRapida
    VERSION 1.0
    QML_FILES
        app/desktop/qml/Main.qml
        app/desktop/qml/Theme.qml
        app/desktop/qml/components/ActionButton.qml
        app/desktop/qml/components/DetailsPanel.qml
        app/desktop/qml/components/FilterPanel.qml
        app/desktop/qml/components/SearchAndPager.qml
        app/desktop/qml/components/SsaTable.qml
        app/desktop/qml/components/StatusPill.qml
)

target_link_libraries(ssa_consulta_rapida PRIVATE
    ssa_presentation
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(ssa_consulta_rapida PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER br.menon.ssa-consulta-rapida
    MACOSX_BUNDLE_BUNDLE_NAME "SSA Consulta Rapida"
    WIN32_EXECUTABLE TRUE
)

install(TARGETS ssa_consulta_rapida
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(SSA_BUILD_TESTS)
    enable_testing()
    include(CTest)
    include(Catch)

    add_executable(ssa_core_tests
        tests/unit/DomainTests.cpp
        tests/unit/SearchParserTests.cpp
        tests/unit/SqlQueryBuilderTests.cpp
    )
    target_link_libraries(ssa_core_tests PRIVATE ssa_core Catch2::Catch2WithMain)
    catch_discover_tests(ssa_core_tests)

    add_executable(ssa_integration_tests
        tests/integration/SqliteRepositoryTests.cpp
    )
    target_link_libraries(ssa_integration_tests PRIVATE ssa_infra Catch2::Catch2WithMain)
    catch_discover_tests(ssa_integration_tests)

    add_executable(ssa_qt_presentation_tests
        tests/smoke/PresentationSmokeTest.cpp
    )
    target_link_libraries(ssa_qt_presentation_tests PRIVATE ssa_presentation Qt6::Test)
    add_test(NAME ssa_qt_presentation_tests COMMAND ssa_qt_presentation_tests)
endif()
