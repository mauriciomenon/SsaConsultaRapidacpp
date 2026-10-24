# Host Windows de validacao

Rotina unica para validar build, testes e pacote Windows numa maquina Windows
nativa acessada por `ssh` (por exemplo uma VM Windows 11 ARM64). O isolamento de
host do `AGENTS.md` continua valendo: nada de Windows a partir do WSL.

## Clone

- Clone canonico em `%USERPROFILE%\gitlab\ssa_consulta_rapida_cpp`, no mesmo
  branch do trabalho, atualizado so com `git pull --ff-only origin <branch>`.
- Um arquivo ainda nao publicado pode ir por `scp` para validar; antes do
  `git pull` seguinte ele volta com `git checkout -- <arquivo>`. O clone termina
  a rodada com `git status --short` vazio.

## Pre-requisitos

- Visual Studio com as ferramentas VC da arquitetura do host. Os scripts abrem o
  Developer PowerShell sozinhos e usam o compilador nativo do host.
- Qt `msvc2022_64` (amd64) ou `msvc2022_arm64` (arm64) em `C:\Qt`.
- SQLite pelo vcpkg do usuario: `vcpkg install sqlite3:<arm64-windows|x64-windows>`
  e `-SQLiteRoot "$env:LOCALAPPDATA\vcpkg\installed\<triplet>"`.
- NSIS para o pacote (`winget install --id NSIS.NSIS -e`).
- Pester 5 so para os testes PowerShell, salvo numa pasta temporaria
  (`Save-PSResource -Name Pester -Path $env:TEMP\ssa_modules`), sem instalar no
  perfil. `tests/scripts/PackageWindows.Tests.ps1` roda em `powershell.exe` 5.1.

## Dados

- O banco operacional nunca vai para o host de validacao.
- Quando um passo exigir banco, usar o fixture de `tools/generate_mem_fixture.sql`.
  Artefato gerado com fixture leva um `NOT_FOR_RELEASE.txt` e nao e distribuido.

## Rodada

1. Rodar so o que o slice exige: build, ctest focado, pacote.
2. Trazer os artefatos por `scp` para fora do repositorio no Mac e conferir com o
   `SHA256SUMS` do pacote.
3. Smoke do pacote sem `QT_QPA_PLATFORM=offscreen`: o pacote leva so o plugin
   `qwindows`, e sem o plugin pedido o Qt para numa caixa de erro. Use
   `--version` e `--smoke-exit-ms <ms>`, que terminam com codigo de saida.

## Limpeza no fim da rodada

- Remover `dist\`, `build\windows\<arch>\<toolchain>\<kit>\release`, o fixture em
  `data\ssas.db`, `%TEMP%\ssa_*`, o Pester temporario e o
  `%USERPROFILE%\.ssaconsultarapida` criado pelos smokes (no host de validacao ele
  so tem logs e preferencias de teste).
- Manter o clone, o build `dev` e o vcpkg do usuario. Conferir o espaco livre do
  disco.

## Armadilhas

- Fechar a sessao `ssh` encerra os processos dela: comando longo fica em primeiro
  plano ou com a sessao aberta ate o fim.
- A VM pode estar suspensa ou desligada: conferir antes de concluir que algo
  travou.
