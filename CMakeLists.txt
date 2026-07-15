cmake_minimum_required(VERSION 3.24)

project(
  SSAConsultaRapidaCpp
  VERSION 0.9.5
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(SSA_BUILD_TESTS "Build tests" ON)

find_package(
  Qt6 6.6 REQUIRED
  COMPONENTS Concurrent
             Core
             Gui
             Qml
             Quick
             QuickControls2
             Sql)
if(SSA_BUILD_TESTS)
  find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
endif()
find_package(SQLite3 REQUIRED)

if(TARGET SQLite3::SQLite3)
  set(SSA_SQLITE_TARGET SQLite3::SQLite3)
elseif(TARGET SQLite::SQLite3)
  set(SSA_SQLITE_TARGET SQLite::SQLite3)
else()
  add_library(ssa_sqlite3 UNKNOWN IMPORTED)
  set_target_properties(
    ssa_sqlite3
    PROPERTIES IMPORTED_LOCATION "${SQLite3_LIBRARIES}"
               INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIRS}")
  set(SSA_SQLITE_TARGET ssa_sqlite3)
endif()

include(cmake/Dependencies.cmake)

qt_standard_project_setup(REQUIRES 6.6)
qt_policy(SET QTP0004 NEW)

add_library(
  ssa_core STATIC
  src/domain/ColumnCatalog.cpp
  src/domain/ColumnValuePriorityPolicy.cpp
  src/domain/SsaImportPolicy.cpp
  src/domain/SsaRelationGraph.cpp
  src/query/SearchParser.cpp
  src/query/AdvancedFilterSqlCompiler.cpp
  src/query/SqlPredicateBuilder.cpp
  src/query/SqlQueryBuilder.cpp
  src/query/SqlQueryText.cpp
  src/query/SsaQueryService.cpp
  src/query/TextFilterToken.cpp
  src/query/TextFilterSqlCompiler.cpp)
target_include_directories(ssa_core PUBLIC src)

add_library(
  ssa_application STATIC
  src/application/FilterPresetService.cpp
  src/application/SsaBrowseService.cpp
  src/application/SsaExecutadasReportService.cpp
  src/application/SsaWorkflowService.cpp
  src/application/UnavailableWorkflowPort.cpp)
target_include_directories(ssa_application PUBLIC src)
target_link_libraries(ssa_application PUBLIC ssa_core)

add_library(ssa_platform_process STATIC src/platform/SupervisedProcess.cpp)
target_include_directories(ssa_platform_process PUBLIC src)
target_link_libraries(ssa_platform_process PUBLIC Qt6::Core)
if(SSA_BUILD_TESTS)
  target_compile_definitions(ssa_platform_process
                             PRIVATE SSA_SUPERVISED_PROCESS_TESTING=1)
endif()

add_library(
  ssa_infra STATIC
  src/infra/export/CsvExportPort.cpp
  src/infra/import/CancelableFileCopy.cpp
  src/infra/import/ImportFileStager.cpp
  src/infra/import/LegacySpreadsheetConverter.cpp
  src/infra/import/SpreadsheetImportWorkflowPort.cpp
  src/infra/import/SsaImportConflictResolver.cpp
  src/infra/import/SsaSpreadsheetHeaderCatalog.cpp
  src/infra/import/SsaSpreadsheetMapper.cpp
  src/infra/import/XlsxPackage.cpp
  src/infra/import/XlsxWorkbookReader.cpp
  src/infra/sqlite/SqliteConnection.cpp
  src/infra/sqlite/SqliteDatabaseValidator.cpp
  src/infra/sqlite/SqliteDerivadasPort.cpp
  src/infra/sqlite/SqliteMaintenancePort.cpp
  src/infra/sqlite/SqliteSsaImportWriter.cpp
  src/infra/sqlite/SqliteSsaRepository.cpp
  src/infra/preferences/JsonUserPreferencesStore.cpp
  src/infra/preferences/JsonFilterPresetStore.cpp
  src/infra/preferences/FilterPreferencesJsonCodec.cpp
  src/infra/preferences/FilterPresetJsonCodec.cpp
  src/infra/preferences/UserPreferencesJsonCodec.cpp)
target_include_directories(ssa_infra PUBLIC src)
target_link_libraries(ssa_infra PUBLIC ssa_core ssa_platform_process
                                       ${SSA_SQLITE_TARGET} Qt6::Core miniz)

add_library(
  ssa_platform STATIC
  src/platform/AppPaths.cpp
  src/platform/DesktopApplicationLauncher.cpp
  src/platform/DesktopExternalCommandPort.cpp
  src/platform/LocalOpenCommandHandler.cpp
  src/platform/OpenPathPolicy.cpp
  src/platform/ScrapReportSamRefreshPort.cpp
  src/platform/SystemThemeResolver.cpp
  src/platform/SamCommandHandler.cpp
  src/platform/SamUrlBuilder.cpp
  src/platform/StartupOptions.cpp)
target_include_directories(ssa_platform PUBLIC src)
target_link_libraries(ssa_platform PUBLIC ssa_core ssa_platform_process
                                          Qt6::Core Qt6::Gui)

add_library(
  ssa_presentation_tables STATIC
  src/presentation/SsaColumnDisplayCatalog.cpp
  src/presentation/SsaTableColumnManager.cpp
  src/presentation/SsaTableDisplayCache.cpp
  src/presentation/SsaTablePageFormatter.cpp
  src/presentation/SsaRecordValueFormatter.cpp
  src/presentation/DetailsFieldsModel.cpp
  src/presentation/SsaTableModel.cpp)
target_include_directories(ssa_presentation_tables PUBLIC src)
target_link_libraries(ssa_presentation_tables PUBLIC ssa_core Qt6::Core
                                                     Qt6::Gui)

add_library(
  ssa_presentation_models STATIC
  src/presentation/BrowseQueryState.cpp
  src/presentation/BrowseInputCoordinator.cpp
  src/presentation/BrowseOrchestrator.cpp
  src/presentation/BrowseSelectionCoordinator.cpp
  src/presentation/BrowseRequestCoordinator.cpp
  src/presentation/BrowsePageLifecycleCoordinator.cpp
  src/presentation/BrowseViewModel.cpp
  src/presentation/ColumnFilterViewModel.cpp
  src/presentation/ColumnSettingsModel.cpp
  src/presentation/CurrentWeekViewModel.cpp
  src/presentation/DatabaseSwitchViewModel.cpp
  src/presentation/DesktopActionsViewModel.cpp
  src/presentation/SearchViewModel.cpp
  src/presentation/AdvancedTextFilterColumnStore.cpp
  src/presentation/AdvancedTextFilterRowModelFactory.cpp
  src/presentation/AdvancedTextFilterSnapshotSynchronizer.cpp
  src/presentation/AdvancedDerivationFilterViewModel.cpp
  src/presentation/AdvancedMacroFilterViewModel.cpp
  src/presentation/AdvancedTextFilterViewModel.cpp
  src/presentation/AdvancedWeekFilterViewModel.cpp
  src/presentation/FilterPanelAdvancedViewModel.cpp
  src/presentation/FilterPanelColumnValueOptions.cpp
  src/presentation/FilterPanelViewModel.cpp
  src/presentation/FilterPanelDistinctValueFetcher.cpp
  src/presentation/FilterPanelDistinctValuesController.cpp
  src/presentation/FilterPanelDistinctValueRequestBuilder.cpp
  src/presentation/FilterPanelSectorViewModel.cpp
  src/presentation/FilterPanelAdvancedState.cpp
  src/presentation/FilterPanelCanonicalizer.cpp
  src/presentation/FilterPreferencesNormalizer.cpp
  src/presentation/FilterPanelStateHelpers.cpp
  src/presentation/FilterPanelState.cpp
  src/presentation/DetailsViewModel.cpp
  src/presentation/DerivadasGraphModel.cpp
  src/presentation/StatusViewModel.cpp
  src/presentation/CommandViewModel.cpp
  src/presentation/MainColumnFlowCoordinator.cpp
  src/presentation/MainPreferenceFlowCoordinator.cpp
  src/presentation/MainRequestFlowCoordinator.cpp
  src/presentation/MainSelectionFlowCoordinator.cpp
  src/presentation/ActionStatusCoordinator.cpp
  src/presentation/ExportViewModel.cpp
  src/presentation/UiSettingsViewModel.cpp
  src/presentation/WorkflowCommandRunner.cpp
  src/presentation/WorkflowCommandViewModel.cpp
  src/presentation/PageQueryCoordinator.cpp
  src/presentation/UserPreferencesCoordinator.cpp
  src/presentation/MainViewModel.cpp)
target_include_directories(ssa_presentation_models PUBLIC src)
target_link_libraries(
  ssa_presentation_models PUBLIC ssa_application ssa_presentation_tables
                                 Qt6::Concurrent Qt6::Core Qt6::Gui)

add_library(ssa_presentation INTERFACE)
target_include_directories(ssa_presentation INTERFACE src)
target_link_libraries(ssa_presentation INTERFACE ssa_presentation_models
                                                 ssa_presentation_tables)

qt_add_executable(
  ssa_consulta_rapida
  app/desktop/DesktopApplicationRuntime.cpp
  app/desktop/DesktopMainViewModelFactory.cpp
  app/desktop/DesktopSmokeCapture.cpp
  app/desktop/DesktopSmokeCaptureFileSystem.cpp
  app/desktop/DesktopSmokeObjectNames.cpp
  app/desktop/DesktopSmokeScreenshotCapture.cpp
  app/desktop/DesktopSmokeWindowLocator.cpp
  app/desktop/DesktopSmokeWindowWaiter.cpp
  app/desktop/main.cpp)
target_include_directories(ssa_consulta_rapida PRIVATE app/desktop)

set_source_files_properties(app/desktop/qml/Theme.qml
                            PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(
  ssa_consulta_rapida
  URI
  SsaConsultaRapida
  VERSION
  1.0
  QML_FILES
  app/desktop/qml/Main.qml
  app/desktop/qml/Theme.qml
  app/desktop/qml/components/ActionButton.qml
  app/desktop/qml/components/AboutDialog.qml
  app/desktop/qml/components/AppCheckBox.qml
  app/desktop/qml/components/AppComboBox.qml
  app/desktop/qml/components/AppSpinBox.qml
  app/desktop/qml/components/AppTextField.qml
  app/desktop/qml/components/AdvancedFilterPanel.qml
  app/desktop/qml/components/AdvancedMacroFilterCard.qml
  app/desktop/qml/components/AdvancedReprogrammingFilterCard.qml
  app/desktop/qml/components/ReprogrammingValuePopup.qml
  app/desktop/qml/components/AdvancedTextValuePopup.qml
  app/desktop/qml/components/AdvancedTextFilterCard.qml
  app/desktop/qml/components/AdvancedTextFilterGrid.qml
  app/desktop/qml/components/AdvancedWeekEmissionCard.qml
  app/desktop/qml/components/AdvancedWeekExecutionCard.qml
  app/desktop/qml/components/DetailsPanel.qml
  app/desktop/qml/components/DetailsRelationNavigator.qml
  app/desktop/qml/components/DerivadasGraph.qml
  app/desktop/qml/components/ColumnFilterPanel.qml
  app/desktop/qml/components/ColumnFilterRow.qml
  app/desktop/qml/components/ColumnSelectorPopup.qml
  app/desktop/qml/components/FilterCard.qml
  app/desktop/qml/components/FilterFieldLabel.qml
  app/desktop/qml/components/FileWorkflowDialogs.qml
  app/desktop/qml/components/FilterTabButton.qml
  app/desktop/qml/components/FilterPanel.qml
  app/desktop/qml/components/FilterSummaryBar.qml
  app/desktop/qml/components/HelpDialog.qml
  app/desktop/qml/components/ColumnConfigList.qml
  app/desktop/qml/components/PagerQuickFilters.qml
  app/desktop/qml/components/PreferencesDialog.qml
  app/desktop/qml/components/SavedFilterControls.qml
  app/desktop/qml/components/SamRefreshDialog.qml
  app/desktop/qml/components/SearchAndPager.qml
  app/desktop/qml/components/SummaryTag.qml
  app/desktop/qml/components/SmokeCaptureBridge.qml
  app/desktop/qml/components/SsaTable.qml
  app/desktop/qml/components/StatusPill.qml
  app/desktop/qml/components/SsaDetailsWindow.qml
  app/desktop/qml/components/ThemeDialog.qml)

target_link_libraries(
  ssa_consulta_rapida PRIVATE ssa_presentation ssa_infra ssa_platform
                              Qt6::Concurrent Qt6::Quick Qt6::QuickControls2)
target_compile_definitions(ssa_consulta_rapida
                           PRIVATE SSA_PROJECT_VERSION="${PROJECT_VERSION}")

add_library(
  ssa_cli STATIC
  app/cli/SsaCliController.cpp app/cli/SsaCliDatabasePath.cpp
  app/cli/SsaCliImportPaths.cpp app/cli/SsaCliPrinter.cpp
  app/cli/SsaCliRequestMapper.cpp app/cli/SsaCliWorkflowRunner.cpp)
target_include_directories(ssa_cli PUBLIC app/cli)
target_link_libraries(ssa_cli PUBLIC ssa_application Qt6::Core)

add_executable(
  ssa_consulta_rapida_cli app/cli/CliServiceFactory.cpp
                          app/cli/SsaCliApplicationRuntime.cpp app/cli/main.cpp)
target_link_libraries(ssa_consulta_rapida_cli PRIVATE ssa_cli ssa_infra)
target_compile_definitions(ssa_consulta_rapida_cli
                           PRIVATE SSA_PROJECT_VERSION="${PROJECT_VERSION}")

set_target_properties(
  ssa_consulta_rapida
  PROPERTIES MACOSX_BUNDLE TRUE
             MACOSX_BUNDLE_GUI_IDENTIFIER br.menon.ssa-consulta-rapida
             MACOSX_BUNDLE_BUNDLE_NAME "SSA Consulta Rapida"
             MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
             MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
             WIN32_EXECUTABLE TRUE)

install(
  TARGETS ssa_consulta_rapida
  BUNDLE DESTINATION .
  RUNTIME DESTINATION bin)

install(TARGETS ssa_consulta_rapida_cli RUNTIME DESTINATION bin)

install(FILES THIRD_PARTY_NOTICES.md third_party/tinted-themes/LICENSE
        DESTINATION share/doc/ssa-consulta-rapida)

if(SSA_BUILD_TESTS)
  enable_testing()
  include(CTest)
  include(Catch)

  add_executable(
    ssa_unit_tests
    tests/unit/ApplicationTests.cpp
    tests/unit/CliControllerTests.cpp
    tests/unit/DomainTests.cpp
    tests/unit/SsaImportPolicyTests.cpp
    tests/unit/SearchParserTests.cpp
    tests/unit/SqlQueryBuilderTests.cpp
    tests/unit/StartupOptionsTests.cpp
    tests/unit/TextFilterTokenTests.cpp)
  target_link_libraries(
    ssa_unit_tests PRIVATE ssa_application ssa_cli ssa_core ssa_platform
                           Catch2::Catch2WithMain)
  catch_discover_tests(ssa_unit_tests PROPERTIES TIMEOUT 15)

  add_library(ssa_theme_lab_core STATIC tools/theme_lab/ThemePaletteLab.cpp)
  target_include_directories(ssa_theme_lab_core
                             PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(ssa_theme_lab_core PUBLIC Qt6::Core Qt6::Gui)

  add_executable(ssa_theme_lab tools/theme_lab/ThemePaletteLabMain.cpp)
  target_link_libraries(ssa_theme_lab PRIVATE ssa_theme_lab_core)

  add_executable(ssa_theme_lab_tests tests/unit/ThemePaletteLabTests.cpp)
  target_link_libraries(ssa_theme_lab_tests PRIVATE ssa_theme_lab_core
                                                    Catch2::Catch2WithMain)
  catch_discover_tests(ssa_theme_lab_tests PROPERTIES TIMEOUT 15)

  add_test(
    NAME ssa_theme_lab_cli_generates_report
    COMMAND
      $<TARGET_FILE:ssa_theme_lab> --input
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/theme_lab --output
      ${CMAKE_CURRENT_BINARY_DIR}/theme-lab-cli-report.json --limit 1
      --source-commit test-fixture)
  set_tests_properties(
    ssa_theme_lab_cli_generates_report
    PROPERTIES PASS_REGULAR_EXPRESSION
               "THEME_LAB input=1 accepted=1 rejected=0 shortlisted=1")

  add_executable(ssa_sqlite_import_crash_probe
                 tests/integration/SqliteImportCrashProbe.cpp)
  target_link_libraries(ssa_sqlite_import_crash_probe PRIVATE ssa_infra)

  add_executable(
    ssa_integration_tests
    tests/integration/ExportPortTests.cpp
    tests/integration/JsonPreferencesStoreTests.cpp
    tests/integration/SpreadsheetImportWorkflowPortTests.cpp
    tests/integration/SsaSpreadsheetHeaderCatalogTests.cpp
    tests/integration/XlsxWorkbookReaderTests.cpp
    tests/integration/SqliteDatabaseValidatorTests.cpp
    tests/integration/SqliteRepositoryTests.cpp)
  target_link_libraries(ssa_integration_tests PRIVATE ssa_infra
                                                      Catch2::Catch2WithMain)
  target_compile_definitions(
    ssa_integration_tests
    PRIVATE
      SSA_SQLITE_CRASH_PROBE_PATH="$<TARGET_FILE:ssa_sqlite_import_crash_probe>"
  )
  add_dependencies(ssa_integration_tests ssa_sqlite_import_crash_probe)
  catch_discover_tests(ssa_integration_tests PROPERTIES TIMEOUT 30)

  add_executable(ssa_qt_presentation_tests
                 tests/smoke/PresentationSmokeTest.cpp)
  target_link_libraries(ssa_qt_presentation_tests PRIVATE ssa_presentation
                                                          Qt6::Test)
  add_test(NAME ssa_qt_presentation_tests COMMAND ssa_qt_presentation_tests)
  set_tests_properties(ssa_qt_presentation_tests PROPERTIES TIMEOUT 30)

  add_executable(ssa_qt_presentation_filter_tests
                 tests/smoke/PresentationFilterSmokeTest.cpp)
  target_link_libraries(ssa_qt_presentation_filter_tests
                        PRIVATE ssa_presentation Qt6::Test)
  add_test(NAME ssa_qt_presentation_filter_tests
           COMMAND ssa_qt_presentation_filter_tests)
  set_tests_properties(ssa_qt_presentation_filter_tests PROPERTIES TIMEOUT 30)

  add_executable(ssa_qt_workflow_runner_tests
                 tests/smoke/WorkflowCommandRunnerTest.cpp)
  target_link_libraries(ssa_qt_workflow_runner_tests PRIVATE ssa_presentation
                                                             Qt6::Test)
  add_test(NAME ssa_qt_workflow_runner_tests
           COMMAND ssa_qt_workflow_runner_tests)
  set_tests_properties(ssa_qt_workflow_runner_tests PROPERTIES TIMEOUT 10)

  add_executable(ssa_qt_sam_refresh_tests
                 tests/integration/SamRefreshWorkflowTests.cpp)
  target_link_libraries(
    ssa_qt_sam_refresh_tests PRIVATE ssa_application ssa_platform
                                     Qt6::Concurrent Qt6::Test)
  target_compile_definitions(ssa_qt_sam_refresh_tests
                             PRIVATE SSA_SUPERVISED_PROCESS_TESTING=1)
  add_test(NAME ssa_qt_sam_refresh_tests COMMAND ssa_qt_sam_refresh_tests)
  set_tests_properties(ssa_qt_sam_refresh_tests PROPERTIES TIMEOUT 15)

  add_executable(ssa_qt_database_switch_tests
                 tests/smoke/DatabaseSwitchViewModelTest.cpp)
  target_link_libraries(ssa_qt_database_switch_tests PRIVATE ssa_presentation
                                                             Qt6::Test)
  add_test(NAME ssa_qt_database_switch_tests
           COMMAND ssa_qt_database_switch_tests)
  set_tests_properties(ssa_qt_database_switch_tests PROPERTIES TIMEOUT 10)

  add_executable(ssa_qt_current_week_tests
                 tests/smoke/CurrentWeekViewModelTest.cpp)
  target_link_libraries(ssa_qt_current_week_tests PRIVATE ssa_presentation
                                                          Qt6::Test)
  add_test(NAME ssa_qt_current_week_tests COMMAND ssa_qt_current_week_tests)
  set_tests_properties(ssa_qt_current_week_tests PROPERTIES TIMEOUT 10)

  add_executable(ssa_qt_preference_lifecycle_tests
                 tests/smoke/PreferenceLifecycleTest.cpp)
  target_link_libraries(ssa_qt_preference_lifecycle_tests
                        PRIVATE ssa_presentation Qt6::Test)
  add_test(NAME ssa_qt_preference_lifecycle_tests
           COMMAND ssa_qt_preference_lifecycle_tests)
  set_tests_properties(ssa_qt_preference_lifecycle_tests PROPERTIES TIMEOUT 15)

  add_executable(ssa_qt_filter_panel_tests tests/smoke/FilterPanelSmokeTest.cpp)
  target_link_libraries(ssa_qt_filter_panel_tests PRIVATE ssa_presentation
                                                          Qt6::Test)
  add_test(NAME ssa_qt_filter_panel_tests COMMAND ssa_qt_filter_panel_tests)
  set_tests_properties(ssa_qt_filter_panel_tests PROPERTIES TIMEOUT 30)

  add_executable(ssa_qml_advanced_popup_tests
                 tests/smoke/AdvancedPopupQmlTest.cpp)
  target_link_libraries(
    ssa_qml_advanced_popup_tests PRIVATE ssa_presentation Qt6::Qml Qt6::Quick
                                         Qt6::QuickControls2 Qt6::Test)
  add_test(NAME ssa_qml_advanced_popup_tests
           COMMAND ssa_qml_advanced_popup_tests)
  set_tests_properties(
    ssa_qml_advanced_popup_tests
    PROPERTIES TIMEOUT 10 RUN_SERIAL TRUE ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=Basic")

  add_executable(ssa_qml_theme_gallery_tests
                 tests/smoke/ThemeGallerySmokeTest.cpp)
  target_link_libraries(
    ssa_qml_theme_gallery_tests PRIVATE Qt6::Qml Qt6::Quick Qt6::QuickControls2
                                        Qt6::Test)
  add_test(NAME ssa_qml_theme_gallery_tests COMMAND ssa_qml_theme_gallery_tests)
  set_tests_properties(ssa_qml_theme_gallery_tests PROPERTIES TIMEOUT 30
                                                              RUN_SERIAL TRUE)
  set_property(
    TEST ssa_qml_theme_gallery_tests
    PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
             "QT_QUICK_CONTROLS_STYLE=Basic" "QT_SCALE_FACTOR=1")

  add_executable(ssa_qml_help_about_tests tests/smoke/HelpAboutQmlTest.cpp)
  target_compile_definitions(
    ssa_qml_help_about_tests
    PRIVATE SSA_BUILD_DIR="${CMAKE_BINARY_DIR}"
            SSA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(
    ssa_qml_help_about_tests PRIVATE ssa_presentation Qt6::Qml Qt6::Quick
                                     Qt6::QuickControls2 Qt6::Test)
  add_test(NAME ssa_qml_help_about_tests COMMAND ssa_qml_help_about_tests)
  set_tests_properties(
    ssa_qml_help_about_tests
    PROPERTIES TIMEOUT 15 RUN_SERIAL TRUE ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=Basic")

  add_test(NAME ssa_gui_version COMMAND ssa_consulta_rapida --version)
  set_tests_properties(
    ssa_gui_version
    PROPERTIES TIMEOUT 10 PASS_REGULAR_EXPRESSION
               "SSA Consulta Rapida ${PROJECT_VERSION}" ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen")

  add_test(NAME ssa_cli_version COMMAND ssa_consulta_rapida_cli --version)
  set_tests_properties(
    ssa_cli_version PROPERTIES TIMEOUT 10 PASS_REGULAR_EXPRESSION
                               "ssa_consulta_rapida_cli ${PROJECT_VERSION}")

  foreach(popup_width IN ITEMS 1580 1180)
    add_test(
      NAME ssa_qml_popup_smoke_${popup_width}
      COMMAND
        ${CMAKE_COMMAND} -DAPP=$<TARGET_FILE:ssa_consulta_rapida>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/popup-smoke-${popup_width}
        -DWIDTH=${popup_width} -DHEIGHT=940 -DMODE=popup -P
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunHermeticGuiSmoke.cmake)
    set_tests_properties(
      ssa_qml_popup_smoke_${popup_width}
      PROPERTIES TIMEOUT 15 RUN_SERIAL TRUE PASS_REGULAR_EXPRESSION
                 "QML_POPUP_SMOKE.*success.*true")
  endforeach()

  add_test(
    NAME ssa_qml_layout_smoke_1180x760
    COMMAND
      ${CMAKE_COMMAND} -DAPP=$<TARGET_FILE:ssa_consulta_rapida>
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/layout-smoke-1180x760 -DWIDTH=1180
      -DHEIGHT=760 -DMODE=layout -P
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunHermeticGuiSmoke.cmake)
  set_tests_properties(
    ssa_qml_layout_smoke_1180x760
    PROPERTIES TIMEOUT 15 RUN_SERIAL TRUE PASS_REGULAR_EXPRESSION
               "QML_LAYOUT_SMOKE.*success.*true")

  add_test(
    NAME ssa_qml_details_timeout
    COMMAND
      ${CMAKE_COMMAND} -DAPP=$<TARGET_FILE:ssa_consulta_rapida>
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/details-timeout-smoke -P
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunDetailsTimeoutSmoke.cmake)
  set_tests_properties(ssa_qml_details_timeout PROPERTIES TIMEOUT 10 RUN_SERIAL
                                                          TRUE)

  if(NOT WIN32)
    add_executable(ssa_advanced_popup_memory_probe
                   tools/advanced_popup_memory_probe.cpp)
    target_compile_definitions(
      ssa_advanced_popup_memory_probe
      PRIVATE SSA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(ssa_advanced_popup_memory_probe
                          PRIVATE Qt6::Qml Qt6::Quick Qt6::QuickControls2)
    add_test(
      NAME ssa_qml_popup_rss
      COMMAND
        ${CMAKE_COMMAND} -DPROBE=$<TARGET_FILE:ssa_advanced_popup_memory_probe>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunPopupRssComparison.cmake)
    set_tests_properties(ssa_qml_popup_rss PROPERTIES TIMEOUT 60 RUN_SERIAL
                                                      TRUE)

    add_executable(ssa_import_memory_probe tools/ssa_import_memory_probe.cpp)
    target_link_libraries(ssa_import_memory_probe PRIVATE ssa_infra Qt6::Core)
    add_test(NAME ssa_import_rss_250k COMMAND ssa_import_memory_probe)
    set_tests_properties(ssa_import_rss_250k PROPERTIES TIMEOUT 240 RUN_SERIAL
                                                        TRUE)

    add_executable(ssa_mem_stress tools/mem_stress.cpp)
    target_link_libraries(ssa_mem_stress PRIVATE ssa_presentation ssa_infra
                                                 Qt6::Core)
    add_test(
      NAME ssa_mem_stress_rejects_invalid_pages
      COMMAND
        ${CMAKE_COMMAND} -DAPP=$<TARGET_FILE:ssa_mem_stress> -DOPTION=--pages
        -DVALUE=abc -DEXPECTED=error:\ --pages\ must\ be\ a\ positive\ integer
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunMemStressArgumentFailure.cmake)
    set_tests_properties(ssa_mem_stress_rejects_invalid_pages PROPERTIES TIMEOUT
                                                                         10)
    add_test(
      NAME ssa_mem_stress_rejects_invalid_page_size
      COMMAND
        ${CMAKE_COMMAND} -DAPP=$<TARGET_FILE:ssa_mem_stress>
        -DOPTION=--page-size -DVALUE=0
        -DEXPECTED=error:\ --page-size\ must\ be\ a\ positive\ integer -P
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunMemStressArgumentFailure.cmake)
    set_tests_properties(ssa_mem_stress_rejects_invalid_page_size
                         PROPERTIES TIMEOUT 10)
    set(MEM_STRESS_MAX_DELTA_ERROR
        "error: --max-footprint-delta-kb must be a positive integer")
    add_test(
      NAME ssa_mem_stress_rejects_invalid_max_delta
      COMMAND
        ${CMAKE_COMMAND} -DAPP=$<TARGET_FILE:ssa_mem_stress>
        -DOPTION=--max-footprint-delta-kb -DVALUE=0
        "-DEXPECTED=${MEM_STRESS_MAX_DELTA_ERROR}" -P
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunMemStressArgumentFailure.cmake)
    set_tests_properties(ssa_mem_stress_rejects_invalid_max_delta
                         PROPERTIES TIMEOUT 10)
  endif()
endif()
