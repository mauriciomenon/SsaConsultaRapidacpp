# Dicionario do schema SQLite

Este documento descreve o contrato compartilhado por GUI, CLI, validador e
importador. A fonte de verdade executavel e `domain::ColumnCatalog`; a GUI e a
CLI nao mantem listas paralelas de colunas.

## Identidade

| Campo | Valor |
| --- | --- |
| Tabela | `ssa_table` |
| Versao do catalogo | `1` |
| Colunas obrigatorias no workbook | `numero_ssa`, `descricao_ssa`, `data_cadastro` |
| Chave logica | `numero_ssa` |
| Coluna derivada | `qtd_derivadas` |

`ColumnCatalog::schemaVersion()` identifica a versao do contrato em codigo; ela
nao e uma tabela de metadados gravada no SQLite atual. A validacao usa a versao
do catalogo e compara as colunas persistidas diretamente.

`ColumnCatalog::schemaColumns()` retorna as colunas persistidas. A lista inclui
tipagem (`Text`, `Integer` ou `DateText`), rotulo e largura padrao. O validador
SQLite compara a tabela contra essa lista completa. O mapper de planilhas usa
`requiredSchemaColumns()` para validar o cabecalho antes de aceitar linhas.

## Uso por camada

| Consumidor | Contrato |
| --- | --- |
| GUI | `schemaColumns()` para labels, tipos, visibilidade e largura |
| CLI | `schemaColumns()` para validacao e diagnostico de colunas |
| Validador SQLite | `schemaTableName()` e `schemaColumns()` |
| Mapper XLSX | `requiredSchemaColumns()` e o catalogo de aliases |
| SQL writer | as mesmas chaves, com `numero_ssa` como identidade |

## Regras de compatibilidade

- Coluna obrigatoria ausente rejeita o workbook antes da escrita SQLite.
- Coluna opcional ausente nao apaga valor existente durante merge incremental.
- Campo inteiro invalido rejeita a linha; nao existe conversao silenciosa para
  zero ou `NULL`.
- `qtd_derivadas` e derivada e nao faz parte da escrita do snapshot importado.
- Uma recriacao deve ocorrer em caminho novo ou depois de renomear o banco
  anterior. O banco anterior permanece disponivel para validacao e rollback.

## API de referencia

```cpp
const auto table = ssa::domain::ColumnCatalog::schemaTableName();
const auto version = ssa::domain::ColumnCatalog::schemaVersion();
const auto columns = ssa::domain::ColumnCatalog::schemaColumns();
const auto required = ssa::domain::ColumnCatalog::requiredSchemaColumns();
```

Qualquer mudanca de schema deve atualizar `ColumnCatalog`, o validador, os
contratos desta pasta e os testes de GUI/CLI antes de alterar a versao.
