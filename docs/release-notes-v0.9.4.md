# SSA Consulta Rapida 0.9.4

## Resumo

A 0.9.4 e um patch de verificacao sobre a 0.9.3. A tag `v0.9.3` permanece
imutavel em `8c0c85d`; esta patch corrige dois blockers encontrados pelo gate
Linux depois da publicacao.

## Correcoes

- Conversor XLS indisponivel retorna `xls converter unavailable` como mensagem
  segura e preserva `LibreOffice soffice executable was not found` somente no
  diagnostico tecnico.
- O teste concreto de retry da limpeza SAM nao tenta simular `EACCES` por bits
  de permissao quando executado como root. Nessa condicao ele e marcado como
  skipped; o contrato de falha continua coberto pelo fake, e o retry real roda
  em POSIX nao-root.

## Build e verificacao macOS

- `./run-macos-smoke-clean`: clean de `build/dev`, configure, build, 241 testes
  e abertura interativa da GUI usando o banco default.
- `./scripts/smoke-macos.sh`: mesmo core em modo offscreen, com screenshot em
  `build/runtime/macos/main.png`.
- `./scripts/build-macos.sh`: build incremental do preset selecionado por
  `SSA_CPP_PRESET`.
- `./scripts/package-macos.sh`: rebuild release, testes e ZIP/DMG.

## Publicacao

A tag anotada `v0.9.4` deve ser criada somente depois dos gates locais, pacote
macOS, refs de `master` em GitLab e Bitbucket e pipeline Linux verde. O remote
`gh` permanece fora da publicacao.
