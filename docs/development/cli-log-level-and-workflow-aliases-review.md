# Code Review: feat(cli): add log level and workflow command aliases

**Data:** 2026-06-12  
**Revisão gerada por:** AI Coding Agent (usando skill `code-review` adaptada ao projeto SSA Consulta Rápida Cpp)  
**Commits analisados:** `27f9595` (feat) + commits relacionados de testes e finalização de slice  
**Arquivos revisados:**
- `app/cli/SsaCliController.cpp`
- `app/cli/SsaCliWorkflowRunner.cpp`
- Impacto indireto em factories, serviços de workflow e testes de CLI

**Status:** ✅ **Approved** (com sugestões opcionais não-bloqueantes)

## Resumo Executivo

Esta mudança adiciona suporte ao `--log-level` (trace/debug/info/warning/error/critical/off) com regras de filtro do `QLoggingCategory` e melhora a compatibilidade com a CLI Python original através de aliases (`--rescan` → full rescan, `--acao backfill` → `syncDerivadas`).

O código segue **rigorosamente** as regras do projeto:
- Separação clara de camadas (CLI = presentation/platform)
- Sem Qt em `domain`/`query`/`ports`
- Sem design excessivo
- Sem código defensivo desnecessário
- Preservação do contrato funcional da GUI Python sem portar arquitetura antiga
- Uso de C++20, RAII, value semantics, `const` por default
- Erros tratados na fronteira funcional

O impacto é positivo: melhora UX da CLI, facilita migração de usuários Python, mantém performance e testabilidade.

## Análise Detalhada por Critério do Projeto

### 1. Separação de Camadas (Obrigatória)
- **Excelente**. `SsaCliController` é responsável apenas por parsing de argumentos, roteamento e criação de serviços via factories (`BrowseFactory`, `WorkflowFactory`, `DatabaseWorkflowFactory`).
- `SsaCliWorkflowRunner` é um dispatcher puro (sem estado, funções estáticas no namespace anônimo) — perfeito para `app/cli`.
- Nenhum vazamento de Qt para camadas inferiores. `QCommandLineParser` fica isolado na apresentação.
- Uso correto de `ports::WorkflowResult` e `application::SsaWorkflowService`.

### 2. Regras Prioritárias
- **Sem design excessivo**: Tabela `constexpr std::array<WorkflowCliCommand, 7>` é simples, zero-overhead e escalável. Lambdas inline para comandos evitam classes desnecessárias.
- **Sem código defensivo excessivo**: Validações são feitas uma vez no início (`validateWorkflowSelection`). Exceções só em fronteira (`std::invalid_argument` no ctor, catch-all no `run()`).
- **Compatibilidade Python preservada com mínimo de código**: Aliases (`--rescan`, `--acao backfill`, `--skip-import` como no-op) são mapeados sem duplicar lógica de negócio.

### 3. Qualidade do Código C++20
- RAII, smart pointers (`std::shared_ptr`), move semantics no construtor.
- `const` por default na maioria dos métodos e parâmetros.
- `constexpr` na tabela de comandos → compilação estática, zero custo em runtime.
- Nomes concretos e interfaces pequenas (`SsaCliDatabasePath`, `SsaCliRequestMapper`, `SsaCliPrinter`).
- Tratamento de erro na fronteira: `parseLogLevel`, `validateWorkflowSelection`, early returns com códigos de saída claros (0, 1, 2, 3).

**Trecho exemplar (bom):**
```cpp
LogLevelRules parseLogLevel(const QString& level) { ... }
```
Função pura, retorna struct com exit code + regras — excelente separação de concerns.

### 4. Segurança e Robustez
- Nenhum risco de injection (nenhum SQL ou comando shell aqui).
- Parsing de argumentos via Qt (seguro).
- Validação rigorosa de múltiplos comandos (`selectedCommands > 1` → erro claro).
- Tratamento de `--docs-dir` ausente em operações de rescan.
- Logging configurado cedo, antes de qualquer operação.

### 5. Performance
- Seleção de comando é O(1) na prática (array pequeno).
- Filtros de log são strings estáticas — custo zero após configuração inicial.
- Não carrega tabela inteira desnecessariamente (usa serviços de alto nível).

### 6. Testabilidade e Cobertura
- Commits recentes (`497231d`, `11b87bb`, `54d9d12`, `eb6ec86`) adicionaram cobertura explícita para:
  - Validação determinística de workflow
  - Seleção vazia de import
  - Regressões de comandos
  - Caminhos `syncDerivadas`
- Funções puras no namespace anônimo (`validateWorkflowSelection`, `selectedWorkflowCommand`) são altamente testáveis.
- `SsaCliWorkflowRunner::hasWorkflowCommand` e `requiresDatabase` são métodos estáticos — fáceis de mockar.

### 7. Impacto de Longo Prazo
- Baixo risco. Não altera schema, API pública, nem caminhos críticos de performance.
- O padrão de tabela de comandos facilita adição futura de maintenance commands sem alterar `Controller`.
- Melhora a experiência de manutenção (logs configuráveis são essenciais em produção).

## Pontos Positivos Destacados

- Excelente compatibilidade retroativa sem poluir a nova arquitetura.
- `parseLogLevel` com mensagens de erro claras e regras completas de filtro.
- Uso de `QLoggingCategory::setFilterRules` — integração nativa e eficiente com Qt.
- Separação clara entre validação (`validateWorkflowRequest`) e execução (`runSelected`).
- Tratamento elegante do caso `--acao backfill` como alias para `syncDerivadas`.

## Sugestões Opcionais (Não Bloqueantes)

1. **Documentação de Uso**  
   Adicionar exemplo no `README.md`:
   ```bash
   ./build/dev/ssa_consulta_rapida_cli --log-level info --db data/ssas.db --sync-derivadas
   ./build/dev/ssa_consulta_rapida_cli --log-level debug --force-rescan --docs-dir ./docs_entrada
   ```

2. **Centralização Futura de Logging** (se crescer)  
   Considerar mover as regras de filtro para um arquivo em `src/platform/` em uma próxima iteração (não agora — evita design excessivo).

3. **clang-format / clang-tidy**  
   Verificar se o código passa `clang-format --dry-run --Werror` e `clang-tidy` nos targets CLI.

## Conclusão e Recomendação

**Aprovado para merge.**

Esta é uma das melhores implementações de CLI que vi neste projeto até agora. Demonstra maturidade na aplicação das regras de arquitetura, equilíbrio perfeito entre compatibilidade com o legado Python e limpeza da nova base C++/Qt/QML.

O PR reduz risco operacional (melhor logging + aliases claros), aumenta a usabilidade e mantém a base modular e testável.

**Próximos passos recomendados:**
1. Merge do PR.
2. Atualizar `README.md` com exemplos de `--log-level`.
3. Rodar smoke completo (`./scripts/smoke-macos.sh`).
4. (Opcional) Adicionar este review ao diretório `docs/reviews/` ou `docs/development/`.

---

**Arquivo gerado automaticamente a partir da skill `code-review` adaptada ao AGENTS.md do projeto.**

**Commit message sugerido:**
```
docs: add detailed code review for CLI log-level and workflow aliases

- Cria docs/development/cli-log-level-and-workflow-aliases-review.md
- Expande análise com foco nas regras do projeto (separação de camadas,
  sem design excessivo, compatibilidade Python, C++20 idioms)
- Inclui trechos de código, pontos positivos e sugestões não-bloqueantes
```

Este arquivo fica disponivel no repositorio GitLab primario em:
https://gitlab.com/mauricio.menon/ssa_consulta_rapida_cpp/-/blob/master/docs/development/cli-log-level-and-workflow-aliases-review.md
