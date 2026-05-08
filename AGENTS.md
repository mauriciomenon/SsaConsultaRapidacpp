# SSA Consulta Rapida Cpp - Agent Rules

## Objetivo

- Criar e manter uma versao C++/Qt/QML modular da GUI SSA Consulta Rapida.
- Preservar contratos funcionais da GUI Python sem portar sua arquitetura interna.
- Manter core sem Qt, GUI sem SQL, infraestrutura sem decisao visual.

## Regras Prioritarias Deste Projeto

- Nao fazer design excessivo.
- Nao escrever codigo defensivo excessivo.
- Em projeto novo, nao preservar compatibilidade antiga sem motivo explicito.
- Estas tres regras prevalecem sobre qualquer pressao por slices pequenos quando a decisao for estrutural.

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

## Codigo

- Codigo, comentarios, nomes de arquivos e docs tecnicas devem ser ASCII.
- Comunicacao de trabalho em PT-BR ASCII.
- C++20, RAII, value semantics, const por default.
- Interfaces pequenas, nomes concretos, ownership explicito.
- Erro tratado por fronteira funcional, nao a cada poucas linhas.

## Testes

- Catch2 para core/query/infra.
- QtTest para presentation.
- Smoke QML para startup e fluxo basico.
- Testes devem validar contrato real, nao apenas "nao trava".

## Validacao Esperada

- `cmake --preset dev`
- `cmake --build --preset dev`
- `ctest --preset dev --output-on-failure`
- `clang-format --dry-run --Werror` quando disponivel.
- `clang-tidy` nos targets principais quando disponivel.
- `qmllint` nos arquivos QML quando disponivel.

