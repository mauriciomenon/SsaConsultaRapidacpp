# Recovery Backlog

## Pendente (fora do escopo desta trilha)

- [PENDING] Implementar sincronizacao completa de derivadas com regras de negocio e fonte externa, incluindo modelagem de grafo/fluxo derivado.
- [PENDING] Adicionar lacunas restantes de paridade da tabela: reorder persistido por drag de colunas, acoes de menu de contexto de linha e acao de reset de sort no header.

## Varredura de codigo (junho 2026) - pendentes

### Concorrencia (mesma classe do bug TSan resolvido)
- [HIGH] [C1] `PageQueryCoordinator` (PageQueryResult, payload mais pesado) nao tem `waitForFinished`+processEvents no destructor. Mesmo race do ResultStore<T> que fixei em 3 outros coordinators.
- [HIGH] [C2] `WorkflowCommandRunner` (WorkflowResult com std::string) sem drain no destructor. Idem.
- [MED] [C3] `MainPreferenceFlowCoordinator` watchers QString/FilterPresetLoadResult ainda nao-triviais - so apliquei o meio-fix (processEvents). Migrar para void-watcher + shared_ptr como nos outros 2.

### Performance estrutural (deferido - exige migration/schema)
- [MED] [P1] `readAll` paginado em export/macro = O(N^2) OFFSET paging. Macro herda pageSize=100. Usar path `pageSize==0` (streaming) que ja existe.
- [MED] [P2] status-last CASE sort (`UPPER(COALESCE(...))<>'STE'` por linha) nao-sargable, forca full-sort. Generated column `_status_is_ste` + composite index resolveria, mas exige migration robusta para DBs antigos (CREATE TABLE IF NOT EXISTS nao recria). Revertido - precisa de ALTER TABLE idempotente + fallback no builder.
- [MED] [P4] Macro report agrupa em memoria (`map<ReportKey,set<string>>` so pra .size()). Deveria ser `GROUP BY ... COUNT(DISTINCT)` em SQL.
- [MED] [P7] Eager date/string formatting: formata 500x12 celulas antes de exibir; QML so renderiza visiveis. Lazy exigiria repensar `SsaTableDisplayValues`/`displayCache_` - ciclo dedicado.

### Qualidade / duplicacao
- [HIGH] [Q1] 5 copias de trim/trimCopy: SearchParser, SsaExecutadasReportService, SsaSpreadsheetMapper, FilterPanelStateHelpers, TextFilterToken. Unificar em helper Qt-free (ex: domain/StringView.h).
- [MED] [Q2] 2 copias de uppercaseCopy: SectorHierarchy (private), SqlQueryText (exported). Unificar.
- [MED] [Q3] `SectorHierarchy::orderedSectors` O(n^2) com std::find em loop - deveria usar unordered_set.
- [MED] [Q4] `SsaImportConflictResolver` double-tracking (seenNumbers set + indexBySsa map) pra mesma condicao. Remover seenNumbers.

### Cleanup / low priority
- [LOW] [L-Q1] `ColumnCatalog::defaultVisible()` morto (0 cobertura). So `defaultVisibleKeys()` e usado.
- [LOW] [L-Q2] `emptyFiles`/`failedFiles` contadores mortos no import (failedFiles so pode ser 0 ou 1).
- [LOW] [L-Q3] null checks redundantes em chaves vindas do proprio catalogo (AdvancedTextFilterRowModelFactory, ColumnFilterViewModel).
- [LOW] [L-Q4] `tokenOperatorForStorage` microfuncao tautologica (Different?Different:Equals). Remover.
- [LOW] [L-A1] `SearchParser` trata `-` inicial como negacao sem escape (nao busca conteudo com hifen inicial).
- [LOW] [L-A2] `kLastIsoWeek=53` aceita semana 53 em anos nao-longos (resultado vazio silencioso).
- [LOW] [L-A3] `LegacySpreadsheetConverter:117` `error` nao limpo apos `remove`.
- [LOW] [L-P1] `FilterPanelViewModel::advancedFilters()/columnFilters()` retornam mapas por valor na hot path. Retornar const&.

## Otimizacao de performance (deferido do ciclo de audit de anti-padroes)

- [RESOLVED] [TSan] Data race em `QArrayDataPointer<char16_t>::deref()` corrigido. Causa raiz: `QFutureWatcher<T>` com T nao-trivial tem race no `ResultStore` durante teardown concorrente com o worker reportando resultado; alem disso `QtConcurrent::run` com cancel+setFuture imediato race o vtable do runnable. Correcoes aplicadas:
  - `UserPreferencesCoordinator`: migrado para `QFutureWatcher<void>` + erro em `std::string` protegido por mutex.
  - `FilterPanelDistinctValueFetcher`: migrado para `QFutureWatcher<void>` + resultado via `shared_ptr` guarded por mutex; requests concorrentes agora enfileirados (pending) em vez de cancel+setFuture imediato.
  - `MainPreferenceFlowCoordinator`: adicionado `processEvents()` apos `waitForFinished` para drenar o signal finished antes do teardown.
  Validado: dev-tsan 100/100 (2x consecutivas).

- [COVERAGE] Analise de gaps de cobertura (78.15% linhas agregado, meta 90%).
  Arquivos com cobertura < 70% (priorizar adicionar testes):
  - `query/SqlPredicateBuilder.cpp` (0%) - critico, compoe WHERE clauses
  - `query/SqlQueryText.cpp` (0%) - validacao/escape de identificadores SQL
  - `query/SqlQueryBuilder.cpp` (~6%) - gerador central de queries (testes unit existe mas nao cobre buildRows/buildCount)
  - `application/SsaBrowseService.cpp` (~1%) - servico de browse principal
  - `domain/SectorHierarchy.cpp` (~6%) - logica de hierarquia de setores
  - `domain/ColumnValuePriorityPolicy.cpp` (~3%) - politica de prioridade de colunas
  - `infra/import/SsaSpreadsheetHeaderCatalog.cpp` (0%) - catalogo de headers de import
  - `infra/preferences/FilterPresetJsonCodec.cpp` (~1%) - codec JSON de presets
  - `ports/*.h` (0-1%) - interfaces, cobertura esperada baixa (headers)
  Gerar relatorio: `bash scripts/generate-coverage.sh` -> build/dev-cov/coverage_html/

- [STYLE] clang-tidy/cppcheck achados de estilo nao corrigidos (codigo pre-existente em SsaTypes.h/ColumnCatalog - refatoracao transversal fora de escopo):
  - `cppcoreguidelines-pro-type-member-init` em varias structs de dominio (AdvancedFilterSpec, SsaPageRequest, etc.)
  - `performance-enum-size` (enums usam int onde uint8 bastaria)
  - `portability-avoid-pragma-once` (pragma once e padrao moderno, mantido)
  - `bugprone-easily-swappable-parameters` (2 params int adjacentes em pageCount)
  - `functionStatic`/`constParameterReference`/`useStlAlgorithm` (varios - baixo impacto)
  Enderecar em ciclo dedicado de hardening de dominio, nao junto a mudancas funcionais.

- [PENDING] [L2] Avaliar FTS5 para general search (`%contains%`). A busca default (MatchMode::Contains) produz `LIKE '%text%'` colapsado em N colunas, que nao e sargable mesmo com indices. FTS5 (virtual table + triggers) tornaria o general search indexado. Mudanca maior: exige schema versionavel, populate do indice no import e manutencao em updates. Adiar ate dataset real demonstrar lentidao.
- [PENDING] [L6] Bind de LIMIT/OFFSET como int64. Hoje bindings sao `std::vector<std::string>` (text) em todo o pipeline; SQLite coerciona em runtime sem custo real, mas o contrato deveria ser tipado. Exige variante de binding (text vs int64) atravessando SqlQuery/bindAll/fakes/testes. Ganho marginal; adiar ate outro refactor do query builder justificar o custo.
- [PENDING] [L7] `recordBySsaNumber` com `SELECT *`. A tela de detalhes mostra todas as colunas, entao `SELECT *` e apropriado hoje. Reavaliar se a tela de detalhes passar a projetar um subconjunto.

- [PENDING] [UX-NAV] Navegacao por setas <> no DetailsPanel e confusa. As setas percorrem a cadeia de derivadas da SSA em exibicao (Current/Mae/Filhas), mas ao carregar uma SSA relacionada a cadeia muda para a dela e o indice restaurado pode apontar para posicao inconsistente. Considerar historico de navegacao (back/forward) ou manter a cadeia original fixa durante a navegacao. Commit 990667c documenta o funcionamento atual em detalhe.
