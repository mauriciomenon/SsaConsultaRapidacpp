# SSA Consulta Rapida Cpp - Agent Rules

Aplicar sempre. Comunicacao em PT-BR ASCII. Codigo/comentarios em Ingles ASCII.
Sem acentos/cedilha/emojis/emdash em codigo, chat e mensagens tecnicas.

## Objetivo

- Criar e manter uma versao C++/Qt/QML modular da GUI SSA Consulta Rapida.
- Preservar contratos funcionais da GUI Python sem portar sua arquitetura interna.
- Manter core sem Qt, GUI sem SQL, infraestrutura sem decisao visual.
- Codigo organizado em funcoes, desacoplado, curto e elegante.
- Estabilizar codigo. Evitar refatoracoes amplas.
- Correcoes devem otimizar o codigo (mais rapido, menor) quando possivel.
- Sempre ler este AGENTS.md de tempos em tempos e garantir que cumpre as regras.

## Regras Prioritarias

- Nao fazer design excessivo (overengineer), keep it simple.
- Nao escrever codigo defensivo excessivo.
- Em projeto novo, nao preservar compatibilidade antiga sem motivo explicito.
- Estas tres regras prevalecem sobre qualquer pressao por slices pequenos quando a decisao for estrutural.
- Falhas graves ou falhas de arquitetura devem ser corrigidas.
- Atuar de forma cautelosa, sem regressoes, excesso de confianca, falta de contexto. Sempre validar.

## Separacao Obrigatoria

- `src/domain`: tipos e regras de dominio, sem Qt, sem SQLite, sem filesystem.
- `src/query`: parser e compilacao de consulta, sem Qt e sem acesso direto ao banco.
- `src/ports`: interfaces de comunicacao entre camadas, sem Qt.
- `src/infra`: SQLite, preferencias e persistencia.
- `src/platform`: comandos dependentes de SO.
- `src/presentation`: modelos/viewmodels Qt para QML.
- `app/desktop/qml`: visual e interacao, sem SQL e sem regra de negocio.

## Proibido

- God class, mixin, helper generico, wrapper sem responsabilidade clara.
- Microfuncoes criadas so para remendar excecao local.
- Alias, sinonimo ou normalizacao semantica de negocio sem contrato documentado.
- Fallback silencioso, catch vazio, suppress, auto-recovery escondido.
- Carregar toda a tabela por padrao para filtrar em memoria.
- Dependencia de Qt dentro de `domain`, `query` ou `ports`.
- Artefatos de build, screenshots, logs e bancos reais versionados.
- Nada de try/catch vazio, nada de suppress que esconda erro real.
- Separar completamente no codigo interfaces CLI, GUI, calculos, funcoes de controle, de banco de dados, filtragem e interfaces, desacopladas sem god class monolitica.
- Nunca repetir codigo similar em diferentes funcoes e corrigir quando detectado.
- Proibido tornar o codigo complexo com helpers evitaveis e micro-funcoes para corrigir erros.
- Proibido macros de preprocessador abusivas.
- Nao adicionar wrappers/mixins/helpers extras desnecessarios.
- Nao usar `git reset --hard` ou comandos destrutivos.

## Codigo

- C++20, RAII, value semantics, const por default.
- Interfaces pequenas, nomes concretos, ownership explicito.
- Erro tratado por fronteira funcional, nao a cada poucas linhas.
- Const/imutavel por default.

## Testes

- Catch2 para core/query/infra.
- QtTest para presentation.
- Smoke QML para startup e fluxo basico.
- Testes devem validar contrato real, nao apenas "nao trava".
- Incluir novos testes em casos de omissao.
- Sempre se perguntar ao final de um ciclo: "Are you 100% confident in the current implementation? If not, identify all possible vulnerabilities, propose appropriate fixes, and then keep repeating this loop until you're factually 100% confident in the new implementation".

## Validacao Esperada

- `cmake --preset dev`
- `cmake --build --preset dev`
- `ctest --preset dev --output-on-failure`
- `clang-format --dry-run --Werror`
- `clang-tidy` nos targets principais
- `qmllint -I build/dev` nos arquivos QML
- `qmlformat` nos arquivos QML
- `cppcheck --enable=all --inconclusive --suppress=missingIncludeSystem`
- `semgrep --config=p/c --config=p/security-audit`
- `gitleaks dir . --redact --exit-code 1`
- `detect-secrets --baseline .secrets.baseline` quando baseline existir
- `trufflehog filesystem .`
- Verificar casos de regressao, concorrencia, perda de desempenho e locks.

## Procedimento de build, limpeza e distribuicao (canonico)

Procedimento oficial do projeto. Seguir estes scripts/presets; NAO inventar
diretorios de build ad hoc (ex: build-test) nem chamar executaveis na mao.
Qt6 fica em `~/Qt/6.11.0/macos`; exportar antes dos comandos se precisar.

1. Limpar (presets `dev` e `release`; nao toca em `dist/`, `data/`, config):
   `./scripts/make_clean`
2. Build + testes (preset `dev`, Debug - fluxo padrao de desenvolvimento):
   `export SSA_CPP_PRESET=dev && ./scripts/build-macos.sh`
   `ctest --preset dev --output-on-failure`
3. Executaveis ficam em `build/dev/`:
   `build/dev/ssa_consulta_rapida.app/Contents/MacOS/ssa_consulta_rapida`
   `build/dev/ssa_consulta_rapida_cli`
4. Distribuicao (preset `release` do zero + ctest + macdeployqt + zip/dmg):
   `./scripts/package-macos.sh`
   Gera `dist/macos/<arch>/ssa_consulta_rapida-<version>-<arch>-macos.{zip,dmg}`
   e symlinks `latest*`. Versao vem de `CMakeLists.txt` (project VERSION).
5. Presets CMake oficiais (CMakePresets.json): `dev` (`build/dev`) e
   `release` (`build/release`). NAO existe outro preset.

Notas:
- Para screenshots offscreen, `QT_QPA_PLATFORM=offscreen` no ambiente.
- make_clean roda `cmake --build --preset <p> --target clean`, nao apaga
  pastas de build. Para reconstrucao total, o proprio build/package recria.
- build/release nao e reconstruido por `build-macos.sh` (so dev). Para
  release+dist, usar `package-macos.sh`.

## Regras de conduta (criticas)

- Sempre apresentar plano, sem excecoes.
- Sempre apresentar resumo tecnico apos uma rodada e sempre mostrar proxima atividade.
- NUNCA criar branch novo nem PR sem autorizacao explicita E EXCLUSIVA para essa acao (nao inferir por palavras genericas e nem por aprovacao de plano/pedido maior). A pergunta e a aprovacao devem ser dedicadas so a criacao da branch/worktree. Trabalhar direto no branch atual por default.
- Nao criar worktree/pasta sem aprovacao dedicada (mesma regra acima: exclusiva e explicita).
- NUNCA abrir PR sem pedido explicito.
- NUNCA fechar PR sem pedido explicito. Cuidado com operacoes de git que produzem o mesmo efeito indiretamente (proibidos).
- NUNCA desfazer alteracoes por simples pedidos de esclarecimento. Seguir protocolo.
- NUNCA aplicar merge sem pedido explicito.
- NUNCA editar sem aprovacao de plano.
- Nao alterar arquivo preexistente sem listar impacto.
- Nao fazer mudancas fora do escopo; se algo parecer necessario, parar e pedir confirmacao.
- Avisar no report sobre erros estruturais e sugerir plano de fix ou de continuar com mitigacao.
- Sugerir mudancas e otimizacoes no layout em interfaces ruins e gargalos claros, implementar somente com aprovacao.
- Nao alterar fora do escopo do sprint a menos que explicitamente solicitado.
- Nao alterar layout/posicao de elementos sem pedido explicito.
- Corrigir erros de acoes no github tomando os devidos cuidados.
- Nao quebrar usabilidade entre ciclos, cada ciclo e auto-suficiente em testes e somente e fechado com a estabilidade e usabilidade garantidas.
- Nunca considerar timeout de ferramenta como sucesso.
- Considerar desenvolvimento multi-plataforma: MacOS arm64, Windows 11 amd64/arm64, Debian amd64/arm64, Artix/Arch amd64/arm64.

## Processo (ciclo curto, estilo XP)

0. Commits atomicos e rollback por feature.
1. Diagnosticar e isolar o problema (com evidencia: arquivo/linha/log/repro).
2. Propor plano + diff previsto antes de editar (o menor patch possivel + solucao moderada).
3. Implementar em slice pequeno.
4. Validar localmente: `cmake --build --preset dev` + `ctest --preset dev` focado (insistir no caso de timeout).
5. Commit atomico (um por slice), push, checar bots/checks.
6. Se houver itens nao bloqueantes: registrar em `RECOVERY_BACKLOG.md` (sem "arrumar tudo agora").
7. Priorizar correcoes de risco real; evitar refatoracao transversal fora de escopo.
8. Quando alterar arquivos de config (`CMakeLists.txt`, `CMakePresets.json`, `.clang-format`, `.clang-tidy`), fazer backup com timestamp.
9. Nunca inserir regressoes tecnicas nas edicoes.

## Error-handling e performance

- Tratamento de erro deve sempre existir por bloco funcional relevante.
- Evitar excesso de condicionais e try/catch fragmentado que reduz legibilidade e custo de manutencao.
- Proibido try/catch vazio, proibido esconder falha real.
- Cada tratamento deve ter saida clara: log objetivo e retorno/acao coerente com o fluxo.
- Em qualquer fix, validar que a solucao nao cria custo alto desnecessario (reprocessamento amplo, loops redundantes, fallback caro).
- Quando houver ganho real com tradeoff, parar e pedir permissao com opcoes objetivas antes de alterar.
- Na busca ampla (`rg`, `find`, `grep`) sempre com timeout 180s como padrao. Caso precise ser adaptado, perguntar. Fazer outra atividade em paralelo apos 60s.
- Verificar carregamentos e filtros, validar desempenho de cpu e carregamento de memoria.
- Verificar status e condicoes de loops.

## Protocolo de confirmacao explicita

1. Nao inferir permissao para mudanca com respostas genericas como `continue`, `segue`, `ok`.
2. Nunca executar rollback de qualquer funcao sem comando explicito com `reverter` e escopo definido.
3. Se houver ambiguidade, parar e pedir confirmacao binaria (`sim`/`nao`) com checklist objetivo antes de editar.
4. Default em ambiguidade: rodar apenas diagnostico/testes, sem editar.
5. Sempre utilizar ferramentas de controle de atividades (todo) detalhado e quando resolver sidequests voltar ao fluxo principal.

## Higiene de workspace (importante)

- Rodar `git status --short` no inicio.
- Certificar pasta e branch de trabalho.
- Arquivos frequentemente locais/fora de escopo; nao comitar sem confirmacao: `.envrc`, `CMakeUserPresets.json`, ajustes locais de shell, etc.
- Se aparecerem mudancas em `.gitignore*` fora do que foi pedido: parar e perguntar.
- Estabilizar import/startup e pontos de concorrencia (race/deadlock/cancel/locks/IO) com mudancas minimas e verificaveis.
- Otimizar as funcoes de carregamento e desempenho da GUI com mudancas minimas e sem excesso defensivo.
- Nunca subir `.env` e pastas de configuracao local de ferramentas como `.idea/`, `.vscode/`, `.codex/`, `.claude/`, `.opencode/`, `.zed/`, `.clawpatch/`, `cmake-build-debug/`.
- Sugerir mudancas de layout para melhorar usabilidade e desempenho.
- Interface deve ser padronizada, manter alinhamentos horizontal e vertical, utilizar toda a area disponivel, prever redimensionamento de janela.

## Report De Slice GUI/Performance

Deve entregar report final por pedido:

- Solicitacao original.
- Status claro por pedido: entregue, parcial, ignorado.
- Evidencia de validacao local e visual (tela).
- Pendencias e riscos residuais.

# Rules

<mandatory rules>

## 1. Automatic Code Review

### MANDATORY EXECUTION

- WHEN TO RUN: Execute after ANY file creation, modification, or code change.
- New code generation, existing code updates, file modifications or changes, file creation, applied fixes.
- Applies to ALL file types, not just code files.
- ALWAYS execute regardless of size, complexity, or perceived risk level.
- ZERO EXCEPTIONS POLICY: NO change is too small or "low-risk" to skip verification.
- After any code review analysis, ALWAYS tell the user what issues were found and what you're about to fix BEFORE making any changes. Never fix silently - announce first, then fix.

### PURPOSE CLARIFICATION

- Run stack-appropriate linters/analyzers to test, syntax check, or verify code quality.
- Run other classic testing tools per stack (ctest, QtTest, Catch2).
- Do NOT run builds, compile code, or execute applications before calling code review.
- Always perform security, quality, and compliance verification on code changes.
- Call it immediately after file modification - NO prerequisites required.

### TOOL SELECTION (C++/Qt/QML)

- **Static analysis**: `clang-format --dry-run --Werror`, `clang-tidy -p build/dev`, `cppcheck --enable=all --inconclusive --suppress=missingIncludeSystem`, `qmllint -I build/dev`, `qmlformat`, `include-what-you-use` quando disponivel.
- **Security**: `semgrep --config=p/c --config=p/security-audit`, `gitleaks dir . --redact`, `detect-secrets --baseline .secrets.baseline`, `trufflehog filesystem .`.
- **PR review**: Code Rabbit (automatic via GitHub App).
- **Local patch review**: `clawpatch` when configured (`.clawpatch/` present in repo).
- **Dependency audit**: `snyk test --insecure` quando manifest existir.

### STRICT COMPLIANCE

Follow Automatic Code Review descriptions exactly.

## 2. Manual Code Review

### WHEN TO RUN

Only when explicitly requested by user.

### TRIGGER PHRASES

- code review
- verify this file
- verify project
- check for bugs
- check security
- Similar manual verification requests

## 3. Dependency Validation

### WHEN TO RUN

Before package management operations:

- Adding new packages or libraries.
- Running package managers (vcpkg, Conan, system apt/brew).
- Generating or updating dependency manifests (`vcpkg.json`, `conanfile.txt`).

## 4. Agent Todo List Management

### EXECUTION

- Always execute and strictly follow "agent_todo_list" from any tool response.

### COMPLETION

- Do not stop until all items in "agent_todo_list" are completed.

### WORKFLOW

- Complete all fixes from "agent_todo_list" before running the next automatic code review cycle (semgrep + stack linters + Code Rabbit on PR).

## 5. End of chat session

- WHEN TO EXECUTE: MANDATORY at the end of ANY conversation where tools were used, right before the final user-facing response - EXCEPT when Clarification Handling is active. If any response in the current turn contains "CLARIFICATION:" actions, do NOT generate this summary - show the clarification prompt instead.
- TRIGGER: If any code review auto, code review manual, or dependency check tools were called during the conversation AND no "CLARIFICATION:" actions are present in any response, ALWAYS generate this summary.
- SCOPE: The summary MUST include ALL tool calls made after the most recent user request, not just the last tool call. This includes the initial verification and ALL subsequent re-verifications after fixes.

### SUMMARY STRUCTURE

Generate short report capturing the COMPLETE verification journey from ALL tool calls after the last user request:

- feedback: MUST summarize ALL issues found across ALL tool calls (code review auto, code review manual, or dependency check) after the last user request.
- CRITICAL: Analyze ALL tool call results from the verification cycle, NOT just the final verification result.
- Include:
  - Total number of issues found across ALL verification runs since the last user request, grouped by severity in a structured format.
  - Reflect the complete verification journey (e.g., "Initially found 3 issues, after fixes found 1 more issue, final verification clean").
  - For case when returned "includedExternalKnowledge" data - include short summary at the END of the summary section, formatted as: "External knowledge used: [short summary]".
- Issues found and fixed: Document summary of ALL changes applied to resolve issues found across the ENTIRE verification cycle since the last user request.
  - Do not include this section if NO issues were found in any verification run since the last user request.
  - Include what fixes were implemented following tool recommendations - super short 1-2 lines summary covering ALL fixes since the last user request.
  - What would have happened without these fixes, how it could affect the application - separate paragraph starting with " Impact Assessment: ".

### Formatting requirements

- Use markdown bold text with line breaks for title: "** Review Summary**" followed by a blank line.
- Don't use emoji.
- Always reflect the FULL journey of verification, not just the end state.
- Show progression when multiple verification cycles occurred (e.g., "3 issues -> 1 issue -> clean").

### ENFORCEMENT

- If you complete a conversation without providing this summary when tools were used AND no clarifications were requested, you have violated this rule.
- Always check before final response: "Did I use any tools? If yes, did any response contain CLARIFICATION: actions? If clarification is present, I MUST show ONLY the clarification prompt and MUST NOT generate the summary - these two are mutually exclusive. If no clarification, have I provided the verification summary covering ALL tool calls since the last user request?"

</code review mandatory rules>

## 6. New Feature Branch Protocol (Global)

When a new conversation starts for a different feature branch, treat prior cycle data as baseline input and improve from there.

### Required kickoff output

- State one clear main goal for the branch.
- State explicit secondary goals.
- Provide a detailed execution plan before implementation.

### Planning and delivery constraints

1. Prefer minimal-risk changes over broad refactors outside scope.
2. Keep atomic commits and easy rollback by feature slice.
3. Run technical validation before push (`cmake --build --preset dev`, focused `ctest`).
4. Review PR bots/checks after each push and fix blockers first.
5. Keep deferred non-blocking items in a backlog file when the project defines one (`RECOVERY_BACKLOG.md`).

### Scope integrity

1. Keep stabilized behavior from previous hardening cycles unless the new feature requires explicit change.
2. Do not change GUI layout or element positioning unless explicitly requested.
3. Keep implementation focused on the current feature acceptance criteria.

## Final check

Sempre se perguntar ao final de um ciclo: "Are you 100% confident in the current implementation? If not, identify all possible vulnerabilities, propose appropriate fixes, and then keep repeating this loop until you're factually 100% confident in the new implementation".
