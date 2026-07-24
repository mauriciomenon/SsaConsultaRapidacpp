# Build And Packaging - Windows

Alvo principal validado: Windows 11 amd64, MSVC x64 e Qt 6.11.x
`msvc2022_64`. O nome do kit Qt identifica a ABI e continua correto com Visual
Studio 2026.

## Dependencias obrigatorias para desenvolvimento

1. Visual Studio 2026 ou Build Tools com `Desktop development with C++`, MSVC
   x64 e Windows SDK:
   https://visualstudio.microsoft.com/downloads/
2. Qt Online Installer com Qt 6.11.x `msvc2022_64`, Qt Base e Qt Declarative:
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
.\scripts\build-windows.ps1
ctest --preset dev --output-on-failure
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

Selecao explicita de kit independente:

```powershell
.\scripts\build-windows.ps1 -QtSubdir llvm-mingw_64
.\scripts\build-windows.ps1 -QtSubdir mingw_64
```

Kits MSVC, LLVM-MinGW e MinGW usam ABIs diferentes. Nunca misture compilador,
bibliotecas ou DLLs entre kits.

## Estado dos compiladores Windows

| Ordem | Compilador | Estado |
| --- | --- | --- |
| 1 | MSVC 19.51, Visual Studio 2026 18.8 | Validado com build, testes e pacote amd64 |
| 2 | LLVM-MinGW 17.0.6 | Kit/toolchain independente disponivel; reconhecido sem gate completo nesta rodada |
| 3 | MinGW GCC 13.1 ou 11.2 | Toolchains independentes; use somente a versao correspondente ao kit Qt; reconhecidos sem gate completo nesta rodada |
| 4 | clang-cl 22.1.3 | Instalado para diagnostico; build com Qt MSVC nao possui gate e nao e suportado nesta versao |

WebAssembly nao e alvo desktop e nao e fallback de build.

Para fazer staging manual somente do executavel de desenvolvimento:

```powershell
$env:QT_DIR = "C:\Qt\6.11.1\msvc2022_64"
$env:Path = "$env:QT_DIR\bin;$env:Path"
& "$env:QT_DIR\bin\windeployqt.exe" .\build\dev\ssa_consulta_rapida.exe
```

Para distribuicao, use sempre o script de pacote abaixo.

## Pacote

```powershell
.\scripts\package-windows.ps1
```

Assim como o build, o comando normal de package inicializa o Developer
PowerShell MSVC com host x64 e target x64 quando esse kit e selecionado.

Saidas canonicas em `dist\windows\<arch>\`:

- `final\`: substituido somente com o conjunto completo;
- `releases\<version>-<commit>\`: entregas imutaveis com `SHA256SUMS`;
- `current.json`: identifica a entrega completa atual.

O diretorio `final\` contem:

- `ssa_consulta_rapida_cpp.exe`: executavel portatil unico e autoextraivel;
- `ssa_consulta_rapida_cpp-installer.exe`: instalador NSIS;
- `ssa_consulta_rapida_cpp.zip`: bundle portatil para extracao.
- `ssa_consulta_rapida_cpp-standalone\`: PE nativo e runtime sem extracao por
  abertura.

Para uso diario, prefira o instalador ou o executavel dentro de
`ssa_consulta_rapida_cpp-standalone\`. O executavel portatil unico descompacta
todo o runtime Qt em `%TEMP%` a cada abertura e deve ser usado somente quando a
entrega em um arquivo unico for necessaria.

Cada conjunto completo e preservado em `releases\<version>-<commit>\`.
Uma nova publicacao do mesmo identificador so e aceita quando todos os hashes
coincidem; `final\` e `current.json` sao atualizados juntos.

Nao inclua bancos reais, configuracoes locais, logs, screenshots ou segredos.
