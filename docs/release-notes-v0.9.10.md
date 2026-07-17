# Release v0.9.10

## Foco

Esta versao consolida a rodada de atomicidade de importacao, protecao das
camadas C++, supervisor de processos, diagnosticos GUI e contratos de menus e
grafo. O schema SQLite permanece ASCII canonico e nao recebeu migration.

## Importacao e banco

- O rescan usa journal-before-move: banco e journal sao publicados antes da
  consolidacao real das fontes.
- Crash, cancelamento e movimento parcial permanecem retomaveis; movimentos
  comprovadamente concluidos saem do journal.
- Staging compara identidade, tamanho e mtime. Desaparecimento depois de um
  snapshot inicial valido e reportado como fonte alterada, nao como falha
  generica de inspecao.
- `SqliteSsaImportWriter` exige capability concedida pelo workflow que possui o
  lock canonico. Isso fecha o uso de producao sem lock e evita auto-deadlock.
- Datas opcionais invalidas chegam a validacao fail-closed; `numero_desvios`
  aceita inteiro ou `Desvio #<inteiro>` e rejeita texto ambiguo.
- O resumo de `qtd_derivadas` usa inicializacao compartilhada, backoff sob lock
  e cleanup direcionado por pai.

## Fronteiras e metadata

- `domain::ColumnDef` contem apenas chave, tipo e participacao na busca geral.
- Os 85 labels gerais ficam em catalogo Qt-free de application; largura e
  visibilidade ficam em presentation.
- GUI e CLI passam cabecalhos CSV pelo request do port. Infra valida a
  cardinalidade e usa chaves canonicas quando um adapter omite labels.
- Labels externos PT/ES/EN continuam normalizados para chaves ASCII pelo
  catalogo de headers da planilha.

## GUI, supervisor e diagnostico

- A GUI mantem 30 eventos recentes copiaveis e persiste logs Qt em tres
  arquivos rotativos de 1 MiB.
- N3 permanece sticky e fail-closed quando a arvore de processos nao pode ser
  comprovadamente encerrada.
- O lifecycle `FailedToStop` encerra e aguarda o leader `QProcess` sem warning,
  preservando descendentes nao comprovados no registry.
- Menus possuem contratos de efeito; popup preserva pointer real; grafo usa o
  modelo C++ real para determinismo, bounds, overlap, teclado e exportacao.

## Validacao local

- Build canonico `dev` e target `all_qmllint`: passaram.
- `ctest --preset dev --output-on-failure`: 443/443 em 71.39 segundos depois
  do bump de versao, incluindo os contratos GUI e CLI de `0.9.10`.
- O caso de substituicao de fonte com mesmo tamanho/mtime passou 50/50.
- Gitleaks, detect-secrets e TruffleHog: zero segredo encontrado.
- CodeRabbit encontrou dois findings no fechamento final; ambos foram
  corrigidos e os gates repetidos.

Windows, Linux, GitHub Actions e pacotes de distribuicao nao sao declarados
como validados nesta nota. GitLab e Bitbucket recebem branch e tag; quotas de CI
continuam dependencias externas.
