## Backlog de recuperacao tecnica

Itens nao bloqueantes para ciclos seguintes:

- Avaliar se existe carga excessiva de QString por celula em cenarios de memoria alta e, se for real, consolidar cache de texto formatado por linha.
- Medir custo da atualizacao de filtros no momento de escrita para reduzir trabalho repetido em cenarios com muitas colunas.
- Auditar `FilterPanelViewModel` para manter limites de responsabilidade claros se novos tipos de filtro avançado forem adicionados.
- Consolidar scripts de smoke para Windows/Debian com opcoes padronizadas de pasta temporaria e caminho de projeto.
- Levantar comportamento de concorrencia entre `PageQueryCoordinator::run` e selecao de linha em altissimo volume com benchmarks.
- Avaliar extracao de `SsaRecord::SchemaIndex` para um conceito explicito de layout/record schema, sem aumentar complexidade antes de medir uso real.
- Medir `ColumnCatalog::find` em fluxos reais e trocar para lookup estatico somente se a busca linear aparecer em hotspot.
- Confirmar semantica do campo `num_reprobaciones` com o schema real antes de renomear chave ou label.
- Medir custo de `FilterPanelState::hasFilterForColumn` durante renderizacao; considerar cache de strings normalizadas se houver custo visivel.
- Avaliar alternativa sem alocacao para `SsaRecord::fields()` antes de usar em loops de renderizacao ou exportacao massiva.
