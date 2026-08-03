# Build And Packaging - Windows

Alvo default validado: Windows 11 amd64, MSVC 19.51 e Qt 6.11.x
`msvc2022_64`. Clang-cl/LLVM e MinGW continuam disponiveis por selecao
explicita.

## Dependencias obrigatorias para desenvolvimento

1. Visual Studio 2026 ou Build Tools com `Desktop development with C++`, MSVC
   x64 e Windows SDK:
   https://visualstudio.microsoft.com/downloads/
2. Qt Online Installer com o kit Qt 6.11.x escolhido, Qt Base e Qt Declarative:
   https://doc.qt.io/qt-6/get-and-install-qt.html
3. CMake 3.24+:
   https://cmake.org/download/
4. Ninja:
   https://github.com/ninja-build/ninja/releases
5. Git:
   https://git-scm.com/download/win
6. vcpkg standalone e SQLite para o triplet do alvo:
   https://learn.microsoft.com/vcpkg/get_started/get-started

O CMake e o Ninja fornecidos em `C:\Qt\Tools` podem ser usados. Evite instalar
copias duplicadas no PATH; o requisito e que `cmake.exe` e `ninja.exe` sejam
localizaveis.

Instalacao canonica do SQLite amd64:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg.exe install sqlite3:x64-windows
$env:VCPKG_ROOT = "C:\vcpkg"
```

Para arm64, use `sqlite3:arm64-windows`. O header e a biblioteca sao usados no
build; `sqlite3.dll` e copiada automaticamente ao lado dos executaveis e entra
no pacote. O usuario final nao instala SQLite.

## Dependencias de pacote

- `windeployqt.exe`, fornecido pelo mesmo kit Qt usado no build.
- `robocopy.exe` e `tar.exe`, incluidos no Windows 11.
- NSIS/`makensis.exe` e obrigatorio para o EXE portatil unico, para o
  instalador e para `-CheckPackage`:
  https://nsis.sourceforge.io/Download

Sem NSIS, `package-windows.ps1` encerra antes do build e informa a dependencia
ausente.

## Preflight

```powershell
.\tools\configure-dev.ps1 -Check
.\tools\configure-dev.ps1 -CheckPackage
```

Os checks sao somente leitura, nao instalam software e nao alteram
`CMakeCache.txt`. Caminhos alternativos podem ser informados com `-QtRoot`,
`-QtDir`, `-QtSubdir` e `-SQLiteRoot`.

Configuracao explicita, quando necessaria:

```powershell
.\tools\configure-dev.ps1
.\tools\configure-dev.ps1 -QtDir "C:\Qt\6.11.1\msvc2022_64"
```

## Build e teste

```powershell
.\build-windows.ps1
ctest --test-dir .\build\windows\amd64\msvc\msvc2022_64\dev --output-on-failure
```

Nao e necessario abrir Developer PowerShell manualmente. Quando MSVC e
selecionado, o script localiza o modulo do Visual Studio e inicializa host x64 e
target x64.

## Ferramenta de teste

Pester e usado somente pelos testes PowerShell, nao pelo runtime do aplicativo.
Instale a versao validada 5.7.1:

```powershell
Install-Module Pester -Scope CurrentUser -RequiredVersion 5.7.1
```

O Pester inbox 3.4 do Windows PowerShell nao suporta a sintaxe usada pelos
testes deste repositorio.

Selecao explicita:

```powershell
.\build-windows.ps1 -Toolchain mingw
.\build-windows.ps1 -Toolchain llvm-mingw
.\build-windows.ps1 -Toolchain msvc
.\build-windows.ps1 -Toolchain llvm
```

Cada toolchain usa um diretorio proprio. Nunca misture cache, compilador,
bibliotecas ou DLLs entre MSVC, clang-cl/LLVM, MinGW e LLVM-MinGW.

## Estado dos compiladores Windows

| Ordem | Compilador | Estado |
| --- | --- | --- |
| 1 | MSVC 19.51 | Default; build release, 610 testes, pacote e startup sem DB validados |
| 2 | clang-cl 22.1.3 + lld-link | Build release, 610 testes, pacote e startup sem DB validados |
| 3 | MinGW GCC 13.1 | Disponivel por selecao explicita; build, 610 testes e smoke sem DB validados |
| 4 | LLVM-MinGW 17.0.6 | Falha cedo: biblioteca C++ nao fornece `std::stop_token` |

WebAssembly nao e alvo desktop e nao e fallback de build.

## Pacote

```powershell
.\scripts\package-windows.ps1
```

Assim como o build, o comando normal de package inicializa o Developer
PowerShell MSVC com host x64 e target x64 quando esse kit e selecionado.

Saidas canonicas em `dist\windows\<arch>\<toolchain>\`:

- `final\`: substituido somente com o conjunto completo;
- `releases\<version>-<commit>-windows-<arch>-<toolchain>\`: entregas
  imutaveis com `SHA256SUMS`;
- `current.json`: identifica a entrega completa atual.

O diretorio `final\` contem:

- `ssa_consulta_rapida_cpp-<version>-<commit>-windows-<arch>-<toolchain>.exe`:
  executavel portatil unico e autoextraivel;
- `...-installer.exe`: instalador NSIS;
- `....zip`: bundle portatil para extracao;
- `...-standalone\`: PE nativo e runtime sem extracao por abertura.

Para uso diario, prefira o instalador ou o executavel dentro de
`ssa_consulta_rapida_cpp-standalone\`. O executavel portatil unico descompacta
todo o runtime Qt em `%TEMP%` a cada abertura e deve ser usado somente quando a
entrega em um arquivo unico for necessaria.

Cada conjunto completo tagueado e preservado em
`releases\<version>-<commit>-windows-<arch>-<toolchain>\`.
Uma nova publicacao do mesmo identificador so e aceita quando todos os hashes
coincidem; `final\` e `current.json` sao atualizados juntos.

O pacote oficial inclui `data\ssas.db` do workspace para iniciar com os dados
entregues. Nao inclua configuracoes locais, logs, screenshots ou segredos.
