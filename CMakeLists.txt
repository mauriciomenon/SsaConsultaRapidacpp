cmake_minimum_required(VERSION 3.24)

project(SSAConsultaRapidaCpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(SSA_BUILD_TESTS "Build tests" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Concurrent Core Gui Qml Quick QuickControls2 Sql Test)
find_package(SQLite3 REQUIRED)

if(TARGET SQLite3::SQLite3)
    set(SSA_SQLITE_TARGET SQLite3::SQLite3)
elseif(TARGET SQLite::SQLite3)
    set(SSA_SQLITE_TARGET SQLite::SQLite3)
else()
    add_library(ssa_sqlite3 UNKNOWN IMPORTED)
    set_target_properties(ssa_sqlite3 PROPERTIES
        IMPORTED_LOCATION "${SQLite3_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIRS}"
    )
    set(SSA_SQLITE_TARGET ssa_sqlite3)
endif()

include(cmake/Dependencies.cmake)

qt_standard_project_setup(REQUIRES 6.6)
qt_policy(SET QTP0004 NEW)

add_library(ssa_core STATIC
    src/domain/ColumnCatalog.cpp
    src/query/SearchParser.cpp
    src/query/SqlPredicateBuilder.cpp
    src/query/SqlQueryBuilder.cpp
    src/query/SqlQueryText.cpp
    src/query/SsaQueryService.cpp
)
target_include_directories(ssa_core PUBLIC src)

add_library(ssa_application STATIC
    src/application/SsaBrowseService.cpp
    src/application/SsaWorkflowService.cpp
    src/application/UnavailableWorkflowPort.cpp
)
target_include_directories(ssa_application PUBLIC src)
target_link_libraries(ssa_application PUBLIC ssa_core)

add_library(ssa_infra STATIC
    src/infra/export/CsvExportPort.cpp
    src/infra/sqlite/SqliteConnection.cpp
    src/infra/sqlite/SqliteMaintenancePort.cpp
    src/infra/sqlite/SqliteSsaRepository.cpp
    src/infra/sqlite/SqliteSchemaInspector.cpp
    src/infra/preferences/JsonUserPreferencesStore.cpp
)
target_include_directories(ssa_infra PUBLIC src)
target_link_libraries(ssa_infra PUBLIC ssa_core ${SSA_SQLITE_TARGET} Qt6::Core)

add_library(ssa_presentation STATIC
    src/platform/DesktopExternalCommandPort.cpp
    src/platform/LocalOpenCommandHandler.cpp
    src/platform/OpenPathPolicy.cpp
    src/platform/SamCommandHandler.cpp
    src/platform/AppPaths.cpp
    src/platform/StartupOptions.cpp
    src/presentation/SsaColumnDisplayCatalog.cpp
    src/presentation/SsaTableColumnManager.cpp
    src/presentation/SsaTableDisplayCache.cpp
    src/presentation/SsaTablePageFormatter.cpp
    src/presentation/SsaTableModel.cpp
    src/presentation/SsaRecordValueFormatter.cpp
    src/presentation/BrowseQueryState.cpp
    src/presentation/BrowseOrchestrator.cpp
    src/presentation/BrowseViewModel.cpp
    src/presentation/ColumnSettingsModel.cpp
    src/presentation/SearchViewModel.cpp
    src/presentation/FilterPanelViewModel.cpp
    src/presentation/DetailsFieldsModel.cpp
    src/presentation/DetailsViewModel.cpp
    src/presentation/StatusViewModel.cpp
    src/presentation/CommandViewModel.cpp
    src/presentation/ExportViewModel.cpp
    src/presentation/UiSettingsViewModel.cpp
    src/presentation/PageQueryCoordinator.cpp
    src/presentation/UserPreferencesCoordinator.cpp
    src/presentation/MainViewModel.cpp
)
target_include_directories(ssa_presentation PUBLIC src)
target_link_libraries(ssa_presentation PUBLIC
    ssa_core
    ssa_application
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
        app/desktop/qml/components/AppCheckBox.qml
        app/desktop/qml/components/DetailsPanel.qml
        app/desktop/qml/components/FilterPanel.qml
        app/desktop/qml/components/PreferencesDialog.qml
        app/desktop/qml/components/SearchAndPager.qml
        app/desktop/qml/components/SsaTable.qml
        app/desktop/qml/components/StatusPill.qml
)

target_link_libraries(ssa_consulta_rapida PRIVATE
    ssa_presentation
    Qt6::Quick
    Qt6::QuickControls2
)

add_library(ssa_cli STATIC
    app/cli/SsaCliController.cpp
    app/cli/SsaCliDatabasePath.cpp
    app/cli/SsaCliPrinter.cpp
    app/cli/SsaCliRequestMapper.cpp
    app/cli/SsaCliWorkflowRunner.cpp
)
target_include_directories(ssa_cli PUBLIC app/cli)
target_link_libraries(ssa_cli PUBLIC
    ssa_application
    Qt6::Core
)

add_executable(ssa_consulta_rapida_cli
    app/cli/main.cpp
)
target_link_libraries(ssa_consulta_rapida_cli PRIVATE
    ssa_cli
    ssa_infra
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

install(TARGETS ssa_consulta_rapida_cli
    RUNTIME DESTINATION bin
)

if(SSA_BUILD_TESTS)
    enable_testing()
    include(CTest)
    include(Catch)

    add_executable(ssa_core_tests
        tests/unit/ApplicationTests.cpp
        tests/unit/CliControllerTests.cpp
        tests/unit/DomainTests.cpp
        tests/unit/SearchParserTests.cpp
        tests/unit/SqlQueryBuilderTests.cpp
    )
    target_link_libraries(ssa_core_tests PRIVATE ssa_application ssa_cli ssa_core Catch2::Catch2WithMain)
    catch_discover_tests(ssa_core_tests)

    add_executable(ssa_integration_tests
        tests/integration/ExportPortTests.cpp
        tests/integration/JsonPreferencesStoreTests.cpp
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
