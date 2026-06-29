# [HIGH_BUG] Xanitizer workflow is not adapted to this C++/Qt repository

**File:** [`.github/workflows/xanitizer.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/xanitizer.yml#L63-L100) (lines 63, 73, 74, 80, 81, 84, 98, 100)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH_BUG  •  **Confidence:** high  •  **Slug:** `other-broken-security-scan`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow contains template Java and JavaScript setup steps, but no pom.xml, package.json, or package lock file exists in the repository. It runs mvn -B compile and npm install before the security analysis, so the workflow will fail before reaching Xanitizer in the current project shape. The SARIF upload also assumes Xanitizer-Findings-List.sarif exists.

## Recommendation

Replace the template Maven/npm steps with the project's canonical CMake build flow or remove this workflow if Xanitizer is not applicable. Ensure the SARIF path is produced before upload.

## Revalidation

**Verdict:** true-positive

O workflow contem template Java e JavaScript: setup-java, mvn -B compile e npm install. A busca por arquivos relevantes nao encontrou pom.xml, package.json ou locks npm/yarn/pnpm no repositorio. Como este projeto e C++/Qt/CMake, esses passos nao representam o fluxo canonico do projeto e tendem a falhar antes da analise Xanitizer no estado atual. A etapa de upload SARIF assume Xanitizer-Findings-List.sarif, mas esse arquivo so existiria se a etapa Xanitizer rodasse e produzisse o resultado esperado. Assim, o workflow nao e um controle de seguranca confiavel para este repositorio. O historico mostra somente a adicao inicial afa909c, sem adaptacao posterior.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
