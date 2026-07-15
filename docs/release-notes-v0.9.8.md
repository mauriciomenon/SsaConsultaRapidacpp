# Release v0.9.8

## Foco

Esta versao estabiliza as decisoes de importacao SSA sem alterar o schema
SQLite existente. O nome original da planilha e a evidencia primaria de
recencia; data da planilha, criacao do arquivo quando disponivel, mtime e
cadastro sao fallbacks ordenados.

## Regras de dados

- O ciclo SSA principal aceita somente XLSX. XLS legado continua no repositorio
  como conversor isolado para uso explicito futuro e nao e chamado pelo
  discovery ou rescan.
- Uma planilha esparsa nao apaga campos ricos de uma fotografia anterior.
  Uma fotografia elegivel mais nova pode atualizar tempos, atraso, parciais,
  espera, desvios e outros indicadores sem forcar regressao de estado.
- `STE` e finalizacao e `SCA` e cancelamento aprovado; ambos sao terminais.
  `SCS` e passageiro. Uma linha terminal nao muda para outro estado.
- SCC, ADI e ASE podem usar somente semana de cadastro quando a data exata nao
  existe. Linhas sem numero, descricao ou data/semana valida sao rejeitadas.
- Valores numericos invalidos sao rejeitados antes da escrita; falha preserva
  o banco pela transacao SQLite.

## Historico de execucao

O schema atual possui uma coluna `descricao_execucao`, portanto a politica
mantem o comportamento de sobrescrever essa coluna. A funcao de dominio ja
reserva a proxima coluna numerada quando uma futura migracao adicionar
`descricao_execucao_2`, `descricao_execucao_3` e assim por diante. A migracao
fica fora desta versao para nao arriscar o banco sagrado.

## Ferramenta complementar

O preset opt-in `dev-arrow` compila `ssa_arrow_snapshot_inspector`, uma leitura
somente leitura parametrizada que materializa um snapshot pequeno em uma
tabela Apache Arrow. O build padrao nao exige Arrow e o inspetor nao executa
SQL de escrita.

## Validacao local

- `export SSA_CPP_PRESET=dev && ./scripts/build-macos.sh`
- `ctest --preset dev --output-on-failure`: 377/377
- `dev-arrow` configurado com Apache Arrow 25.0.0 e smoke concluido
- Scanners de codigo e segredos executados no escopo alterado

Windows e Linux CI continuam dependencias externas e nao sao declarados como
validados nesta nota.
