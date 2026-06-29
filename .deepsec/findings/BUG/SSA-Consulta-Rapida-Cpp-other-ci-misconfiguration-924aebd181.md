# [BUG] SecurityCodeScan workflow targets .NET projects that are absent

**File:** [`.github/workflows/securitycodescan.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/securitycodescan.yml#L31-L35) (lines 31, 32, 34, 35)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** BUG  •  **Confidence:** high  •  **Slug:** `other-ci-misconfiguration`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The current repository is a C++/Qt/CMake project and the scan found no `.csproj`, `.vbproj`, or `.sln` files. The `dotnet restore` and `dotnet build --no-restore` steps therefore do not have a project to operate on, making this workflow fail or provide no security coverage.

## Recommendation

Remove this workflow for this repository, or gate it behind path filters and a manifest check if .NET projects are added later.

## Revalidation

**Verdict:** true-positive

O workflow declara que integra SecurityCodeScan para C# e VB.NET e executa dotnet restore seguido de dotnet build --no-restore. A busca por arquivos rastreados nao encontrou .csproj, .vbproj, .sln, .cs ou .vb no repositorio. Isso confirma que a base atual e um projeto C++/Qt/CMake sem alvo .NET para esse workflow. Na pratica, os passos dotnet nao tem projeto para restaurar ou compilar, entao o workflow tende a falhar ou nao produzir cobertura de seguranca significativa. Nao ha alteracao local ou commit posterior que adicione manifests .NET ou gateie esse workflow.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
