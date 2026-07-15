# Release v0.9.9

## Foco

Esta versao fecha riscos de importacao encontrados na revalidacao pos-v0.9.8.
O banco atual continua protegido por merge seletivo, transacoes SQLite e
consolidacao somente apos commit.

## Regras corrigidas

- A copia de staging compara tamanho, mtime e identidade do arquivo. Uma
  substituicao por outro arquivo com os mesmos metadados nao e publicada.
- `STE` e `SCA` continuam terminais. Snapshots elegiveis podem enriquecer
  semana, responsaveis, execucao simples, descricao, tempos, atrasos,
  parciais, espera e desvios sem apagar campos ausentes.
- `SCC`, `ADI` e `ASE` aceitam descricoes apos o codigo e usam semana quando a
  data de cadastro nao existe. `SCS` continua transitorio.
- `emission_datetime` vence `issue_datetime`; issue e usado por linha somente
  quando emissao esta vazia. Falha de parsing continua fail-closed.
- XLS legado permanece fora do ciclo SSA principal. O conversor isolado nao e
  dependencia de discovery ou rescan.

## Distribuicao

- `resources/` e rastreado no Git.
- `app_icon.icns` e incluido no bundle macOS; PNG, SVG e desktop entry entram
  nos pacotes Unix; ICO e recurso Windows entram no instalador.
- O contrato de versao segue `0.9.9`. Nenhuma tag `1.0` sera criada sem ordem
  explicita.

## Validacao local

- `export SSA_CPP_PRESET=dev && ./scripts/build-macos.sh`
- `ctest --preset dev --output-on-failure`: 396/396
- Testes de corrida de staging passam para mtime alterado e identidade
  substituida com tamanho/mtime iguais.
- Icone verificado no bundle, ZIP e DMG locais.

Windows e Linux CI continuam dependencias externas e nao sao declarados como
validados nesta nota.
