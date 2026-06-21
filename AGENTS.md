# SSA Consulta Rapida Cpp - Agent Rules

Regras universais (multi-stack Python/C/C++/Java/Node/Go) em
`~/.config/opencode/AGENTS.md`. Este arquivo adiciona apenas regras
especificas do projeto C++/Qt/QML.

## Objetivo

- Criar e manter uma versao C++/Qt/QML modular da GUI SSA Consulta Rapida.
- Preservar contratos funcionais da GUI Python sem portar sua arquitetura interna.
- Manter core sem Qt, GUI sem SQL, infraestrutura sem decisao visual.

## Separacao Obrigatoria

- `src/domain`: tipos e regras de dominio, sem Qt, sem SQLite, sem filesystem.
- `src/query`: parser e compilacao de consulta, sem Qt e sem acesso direto ao banco.
- `src/ports`: interfaces de comunicacao entre camadas, sem Qt.
- `src/infra`: SQLite, preferencias e persistencia.
- `src/platform`: comandos dependentes de SO.
- `src/presentation`: modelos/viewmodels Qt para QML.
- `app/desktop/qml`: visual e interacao, sem SQL e sem regra de negocio.

## Proibido (especifico)

- Carregar toda a tabela por padrao para filtrar em memoria.
- Dependencia de Qt dentro de `domain`, `query` ou `ports`.

## Codigo

- C++20, RAII, value semantics, const por default.
- Sem acentos/cedilha/emojis/emdash em codigo, chat e mensagens tecnicas.

## Testes

- Catch2 para core/query/infra.
- QtTest para presentation.
- Smoke QML para startup e fluxo basico.
- Testes devem validar contrato real, nao apenas "nao trava".

## Validacao Esperada

- `cmake --preset dev`
- `cmake --build --preset dev`
- `ctest --preset dev --output-on-failure`
- `clang-format --dry-run --Werror`
- `clang-tidy` nos targets principais
- `qmllint -I build/dev` nos arquivos QML
- `cppcheck --enable=all --inconclusive --suppress=missingIncludeSystem`
- `semgrep --config=p/c --config=p/security-audit`
- `gitleaks dir . --redact --exit-code 1`
- Verificar casos de regressao, concorrencia, perda de desempenho e locks.
