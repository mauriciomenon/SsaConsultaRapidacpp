# Recovery Backlog

## Pendente (fora do escopo desta trilha)

- [LOW] [QT-ROLENAMES-CACHE] Medir e, se houver ganho comprovado, armazenar
  `roleNames()` estavel por modelo para evitar reconstrucoes pequenas. Nao
  alterar o contrato Qt nem adicionar cache sem evidencia de hot path.

- [LOW] [MINIZ-CMAKE-DEPRECATION] O CMake do source cache do miniz declara
  compatibilidade anterior a 3.10 e gera warning em CMake recente. Nao editar
  `.deps-cache/miniz-src/CMakeLists.txt`: o diretorio e gerado e ignorado. Tratar
  somente ao atualizar o pin do miniz, depois de validar build e pacote em todas
  as plataformas suportadas.

- [PENDING] Implementar sincronizacao completa de derivadas com regras de negocio e fonte externa, incluindo modelagem de grafo/fluxo derivado.
- [PENDING] Adicionar lacunas restantes de paridade da tabela: reorder persistido por drag de colunas, acoes de menu de contexto de linha e acao de reset de sort no header.
- [LOW] [IMPORT-CONSOLIDATION-TOCTOU] A consolidacao rejeita diretorios
  symlink e usa rename atomico sem sobrescrita, mas existe uma janela entre
  `symlink_status()` e o rename por pathname. Um escritor local adversarial
  poderia trocar o diretorio nesse intervalo. O workspace operacional e
  confiavel na 0.9.2; hardening futuro deve vincular a identidade do diretorio
  validado a operacao de movimentacao por handle, com desenho multiplataforma.
- [LOW] [SAM-LIMIT-200] Cada rodada REST da 0.9.2 solicita no maximo 200
  registros por setor. Avaliar paginacao do `scrap_report` antes de usar o
  fluxo para setores que excedam esse volume.
- [LOW] [SAM-SINGLE-FLIGHT-PROCESS] O single-flight da atualizacao SAM vale por
  instancia da aplicacao. Duas instancias podem executar atualizacoes ao mesmo
  tempo. Coordenacao entre processos fica adiada ate existir necessidade
  operacional comprovada.

## Varredura de codigo (junho 2026) - pendentes

### Performance estrutural (deferido - exige migration/schema)
- [MED] [P2] status-last CASE sort (`UPPER(COALESCE(...))<>'STE'` por linha) nao-sargable, forca full-sort. Generated column `_status_is_ste` + composite index resolveria, mas exige migration robusta para DBs antigos (CREATE TABLE IF NOT EXISTS nao recria). Revertido - precisa de ALTER TABLE idempotente + fallback no builder.
- [MED] [P4] Macro report agrupa em memoria (`map<ReportKey,set<string>>` so pra .size()). Deveria ser `GROUP BY ... COUNT(DISTINCT)` em SQL.
- [MED] [P7] Eager date/string formatting: formata 500x12 celulas antes de exibir; QML so renderiza visiveis. Lazy exigiria repensar `SsaTableDisplayValues`/`displayCache_` - ciclo dedicado.

### Qualidade / duplicacao
- [HIGH] [Q1] 5 copias de trim/trimCopy: SearchParser, SsaExecutadasReportService, SsaSpreadsheetMapper, FilterPanelStateHelpers, TextFilterToken. Unificar em helper Qt-free (ex: domain/StringView.h).
- [MED] [Q4] `SsaImportConflictResolver` double-tracking (seenNumbers set + indexBySsa map) pra mesma condicao. Remover seenNumbers.

### Cleanup / low priority
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
  - `MainPreferenceFlowCoordinator`, `PageQueryCoordinator` e `WorkflowCommandRunner`: migrados para `QFutureWatcher<void>` com estado compartilhado sincronizado; teardown sem `processEvents()`.
  Validado: dev-tsan 100/100 (2x consecutivas).

- [COVERAGE] Analise de gaps de cobertura (78.15% linhas agregado, meta 90%).
  Arquivos com cobertura < 70% (priorizar adicionar testes):
  - `query/SqlPredicateBuilder.cpp` (0%) - critico, compoe WHERE clauses
  - `query/SqlQueryText.cpp` (0%) - validacao/escape de identificadores SQL
  - `query/SqlQueryBuilder.cpp` (~6%) - gerador central de queries (testes unit existe mas nao cobre buildRows/buildCount)
  - `application/SsaBrowseService.cpp` (~1%) - servico de browse principal
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

- [STYLE] [CLAZY-BASELINE] Scan `level0,level1` encontrou 32 warnings unicos
  preexistentes. Principais grupos: 8 `range-loop-detach`, 5 includes diretos de
  modulos Qt, 9 tipos/getters moc, 7 `QString` globais estaticas, 2 `QString::arg`
  e 1 lambda sem contexto. O gate reviewdog filtra apenas linhas alteradas para
  impedir nova divida. Corrigir a baseline em slice C++ dedicado, com QtTest
  focado por grupo.

- [PARTIAL] [SEMGREP-CPP-PARSER] Semgrep CE 1.169.0 conclui o scan sem finding,
  mas emite 63 diagnosticos de parser no C++/Qt: 46 `PartialParsing`, 16
  `Other syntax error` e 1 `Syntax error`. Macros Qt, preprocessamento e C++
  moderno reduzem a cobertura semantica. Manter clang-tidy, clazy, cppcheck e
  CodeQL como compensacao. As seis regras QML locais possuem fixtures RED/GREEN
  e passam integralmente; criar regra QQmlSA apenas quando existir contrato
  semantico que a busca lexical nao represente com seguranca.

- [PENDING] [L2] Avaliar FTS5 para general search (`%contains%`). A busca default (MatchMode::Contains) produz `LIKE '%text%'` colapsado em N colunas, que nao e sargable mesmo com indices. FTS5 (virtual table + triggers) tornaria o general search indexado. Mudanca maior: exige schema versionavel, populate do indice no import e manutencao em updates. Adiar ate dataset real demonstrar lentidao.
- [PENDING] [L6] Bind de LIMIT/OFFSET como int64. Hoje bindings sao `std::vector<std::string>` (text) em todo o pipeline; SQLite coerciona em runtime sem custo real, mas o contrato deveria ser tipado. Exige variante de binding (text vs int64) atravessando SqlQuery/bindAll/fakes/testes. Ganho marginal; adiar ate outro refactor do query builder justificar o custo.
- [PENDING] [L7] `recordBySsaNumber` com `SELECT *`. A tela de detalhes mostra todas as colunas, entao `SELECT *` e apropriado hoje. Reavaliar se a tela de detalhes passar a projetar um subconjunto.

- [PENDING] [UX-NAV] Navegacao por setas <> no DetailsPanel e confusa. As setas percorrem a cadeia de derivadas da SSA em exibicao (Current/Mae/Filhas), mas ao carregar uma SSA relacionada a cadeia muda para a dela e o indice restaurado pode apontar para posicao inconsistente. Considerar historico de navegacao (back/forward) ou manter a cadeia original fixa durante a navegacao. Commit 990667c documenta o funcionamento atual em detalhe.

- [PENDING] [UX-TABLE] Usuario reporta linhas verticais "grossas" entre colunas de dados da tabela, mas analise de pixels no screenshot offscreen (3 medicoes) nao encontra linhas verticais estruturais - apenas pixels de texto. Pode ser problema de DPI/scaling/font rendering no monitor do usuario ou versao compilada intermediaria. Investigar com o usuario apontando exatamente onde ve as linhas em zoom.

- [PENDING] [UX-GRID-ALIGN] Apos o slice `e348c32` (merge de Macro e Reprogramacoes no grid), 3 refinos pendentes (reportados por usuario em validacao visual):
  1. **Macro card texto desalinhado**: o texto/label da Macro card esta "para cima" (topo), fora do alinhamento vertical dos demais titulos (Setor emissor, Prioridade emissao, etc). A Macro card usa `GridLayout` interno enquanto as text cards usam `ColumnLayout`. Padronizar para mesma baseline vertical.
  2. **Macro card texto deve ir ao mesmo lugar que os demais**: o titulo da Macro card deve estar na MESMA posicao relativa que os titulos das text cards (canto superior esquerdo, mesma fonte/tamanho). Hoje esta em posicao diferente.
  3. **Reprogramming card com espaco maior**: a Reprogramacoes card tem espaco interno maior que as outras (provavelmente por causa do `cardHeight: 70` vs `textCellHeight: 56`). Padronizar a altura para `56` igual as outras, ou revisar o layout interno para nao sobrar espaco.
  Cuidado: cada refino deve ser slice separado e cirurgico, sem quebrar o reflow do Flow + Repeater do `AdvancedTextFilterGrid.qml`.

## Consolidacao GUI QML/Qt (julho 2026) - pendentes

- [PENDING] [THEME-PY-AA] Revisar em slice proprio os 18 contrastes AA legados
  em `classicopy`, `darkpy`, `solarized-darkpy`, `solarized-lightpy`,
  `mint-lightpy` e `paperpy`. Este ciclo preserva as paletas Python importadas
  ipsis litteris e aplica o gate novo somente aos temas nativos adicionados.
- [PENDING] [GUI-CONTRACT-WEEK-DERIVATION] Decidir se a GUI deve restaurar o filtro generico de semana e o seletor de derivacao descritos em `docs/contracts/gui-behavior.md`. O runtime atual expoe apenas os cards de emissao e execucao e nao oferece controle visual para `derivationMode`. Nao alterar o layout ate uma decisao de produto explicita.
- [PENDING] [TYPESCALE-POINTSIZE] Migrar TypeScale de `font.pixelSize` para `font.pointSize` para respeitar a escala de fonte/DPI do SO (acessibilidade - fonte grande). Impacto: revalidar todos os 36 QML em Retina/HiDPI e telas com fonte do SO em Large. Slice dedicado.

## Cobertura de seguranca consolidada (julho 2026)

- Scanners removidos de Python, Node e DAST nao foram restaurados porque o repositorio nao possui superficie Python, Node ou servico HTTP implantado. A cobertura aplicavel permanece em `semgrep.yml`, `devskim.yml`, `defender-for-devops.yml`, `codeql.yml`, `dependency-review.yml` e `ci.yml`, com PR e agenda onde suportado.
- [EXTERNAL] Aguardar a restauracao da conta GitHub, hoje bloqueada por HTTP 403,
  antes de criar/configurar o environment GitHub `release` com revisores
  obrigatorios. O YAML referencia esse environment, mas essa dependencia nao
  bloqueia publicacao ou CI no GitLab e Bitbucket. Ver `ROUND_STATUS.md`.
