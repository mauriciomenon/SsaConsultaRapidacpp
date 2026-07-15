# SSA Consulta Rapida 0.9.7

## Resumo

A 0.9.7 completa os fluxos opcionais planejados depois da importacao SSA
segura: valida a atualizacao SAM ate SQLite, adiciona importacao explicita de
derivadas, centraliza a ordem de valores prioritarios e corrige a ancoragem do
seletor de colunas.

## Atualizacao SAM

- Todos os artefatos de setor sao staged antes de abrir a escrita atomica.
- Cada workbook, schema e suas contagens de manifesto e linhas fisicas sao
  validados dentro da unica transacao.
- O commit ocorre somente depois que todos os setores passam; falha em qualquer
  setor rejeita o lote completo sem mutacao parcial.
- Resultado com exatamente 200 linhas e rejeitado como potencialmente truncado.
- A funcionalidade permanece desabilitada por padrao e nao aceita nem persiste
  senha, token ou segredo.

## Importacao de derivadas

- CSV, TXT, TSV, XLSX e XLSM sao aceitos por selecao explicita.
- XLS legado continua disponivel somente por selecao e preflight visivel do
  LibreOffice; ele nao participa do discovery ou rescan SSA.
- O parser reconhece a matriz visual usada pela origem Python, todas as abas e
  headers Unicode normalizados.
- Self-loop, multiparent e filho inexistente rejeitam o lote. Arestas repetidas
  sao deduplicadas e pais ausentes em lote parcial sao preservados.
- Cancelamento e observado durante leitura e mapping; a publicacao SQLite e
  transacional.

## GUI e ordenacao

- Setores e responsaveis usam uma unica ordem prioritaria no display e no SQL.
- O seletor de colunas recebe o `MenuItem` real que o abriu, usa o overlay e
  resolve posicao e tamanho em uma unica funcao com clamp nas bordas.
- Fechar o menu de contexto nao fecha o popup recem-aberto. Resize com o popup
  visivel recalcula a geometria sem bloquear a GUI.

## Build canonico

```bash
./scripts/make_clean
export SSA_CPP_PRESET=dev
./scripts/build-macos.sh
ctest --preset dev --output-on-failure
./scripts/package-macos.sh
```

Validacao local antes do commit documental: 366 de 366 testes, screenshots
offscreen em 1180x760 e 1500x900, scanners sem achados e revisao independente
final limpa. Publicar somente em `origin` e `bitbucket`.

## Limites declarados

- Windows so sera declarado validado com execucao real no Windows.
- Linux depende do pipeline GitLab. Cota ou indisponibilidade externa nao e
  tratada como sucesso.
- Paginacao SAM permanece evolucao futura; o guard de truncamento impede
  aceitar silenciosamente um resultado no limite atual.
