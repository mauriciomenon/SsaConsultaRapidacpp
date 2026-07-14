# SSA Consulta Rapida 0.9.5

## Resumo

A 0.9.5 e um patch de integridade da importacao e verificacao de cancelamento
sobre a 0.9.4. As tags anteriores permanecem imutaveis.

## Importacao e atomicidade

- Full rescan sem arquivo importavel, sem header reconhecido ou sem linha valida
  preserva o estado SQLite anterior e mantem as fontes no diretorio de entrada.
- Artefatos copiados ou convertidos pela operacao sao removidos em cancelamento
  e falha antes do commit; arquivos preexistentes nunca sao removidos pelo
  stager.
- Falha de cleanup possui status explicito. Cancelamento limpo nao vira falha e
  falha primaria nao e mascarada por um stop concorrente.
- Falhas de preflight e leitura de diretorios usam `Failed`, razao publica
  segura e detalhe tecnico no diagnostico. Symlink e arquivo no lugar de
  diretorio continuam rejeicoes de politica.
- Contagens de falha de consolidacao usam numero de fontes nas falhas de
  preflight; o total `failed` inclui conversoes XLS malsucedidas.

## Conversao XLS e processos

- O teste de conversor ausente injeta um caminho inexistente e nao depende do
  `PATH` da maquina.
- Override de LibreOffice precisa apontar para arquivo executavel; diretorio com
  permissao de busca nao e aceito como ferramenta.
- Copia e conversao distinguem `Canceled`, `Failed` e `CleanupFailed`.
- A barreira bem-sucedida do supervisor limpa seu estado e permite nova
  operacao; timeout ou falha continuam fail-closed.

## Tooling e diagnosticos IDE

- Markdownlint usa `MD024.siblings_only`, adequado a headings repetidos entre
  releases do changelog.
- Warnings em `.serena` e `.zcode` pertencem a configuracoes locais nao
  versionadas.
- O warning CMake em `.deps-cache/miniz-src` pertence ao source cache upstream e
  fica registrado para uma futura atualizacao do pin, nunca para edicao local do
  cache.

## Build e publicacao

O fluxo macOS usa somente os comandos canonicos:

```bash
export SSA_CPP_PRESET=dev
./scripts/build-macos.sh
ctest --preset dev --output-on-failure
./scripts/package-macos.sh
```

A tag `v0.9.5` deve ser criada somente depois dos gates locais, pacote, refs de
`master` e pipelines obrigatorios. Publicar em `origin` e `bitbucket`; nao
publicar em `gh` enquanto a conta estiver suspensa.
