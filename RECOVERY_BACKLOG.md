# Recovery Backlog

## Pendente (fora do escopo desta trilha)

- [PENDING] Implementar sincronizacao completa de derivadas com regras de negocio e fonte externa, incluindo modelagem de grafo/fluxo derivado.
- [PENDING] Adicionar lacunas restantes de paridade da tabela: reorder persistido por drag de colunas, acoes de menu de contexto de linha e acao de reset de sort no header.

## Otimizacao de performance (deferido do ciclo de audit de anti-padroes)

- [RESOLVED] [TSan] Data race em `QArrayDataPointer<char16_t>::deref()` corrigido. Causa raiz: `QFutureWatcher<T>` com T nao-trivial tem race no `ResultStore` durante teardown concorrente com o worker reportando resultado; alem disso `QtConcurrent::run` com cancel+setFuture imediato race o vtable do runnable. Correcoes aplicadas:
  - `UserPreferencesCoordinator`: migrado para `QFutureWatcher<void>` + erro em `std::string` protegido por mutex.
  - `FilterPanelDistinctValueFetcher`: migrado para `QFutureWatcher<void>` + resultado via `shared_ptr` guarded por mutex; requests concorrentes agora enfileirados (pending) em vez de cancel+setFuture imediato.
  - `MainPreferenceFlowCoordinator`: adicionado `processEvents()` apos `waitForFinished` para drenar o signal finished antes do teardown.
  Validado: dev-tsan 100/100 (2x consecutivas).

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
