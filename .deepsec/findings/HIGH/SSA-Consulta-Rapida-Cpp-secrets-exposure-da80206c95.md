# [HIGH] PR-controlled Maven or npm code can run before the Xanitizer license secret is used

**File:** [`.github/workflows/xanitizer.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/xanitizer.yml#L38-L87) (lines 38, 60, 73, 74, 80, 81, 84, 85, 87)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH  •  **Confidence:** medium  •  **Slug:** `secrets-exposure`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow runs on pull_request, checks out PR code, then executes mvn -B compile and npm install before invoking the Xanitizer action with XANITIZER_LICENSE. A same-repo PR author could add a pom.xml, Maven plugin, package.json lifecycle script, or background process that persists into the later secret-bearing action step and reads action input environment from same-user runner processes. This can expose the Xanitizer license secret.

## Recommendation

Do not pass XANITIZER_LICENSE in a job after executing PR-controlled build/install hooks. Run this licensed scan only on trusted push/schedule events, or split untrusted build validation from secret-bearing analysis. Remove npm lifecycle execution unless explicitly needed, and kill any background processes before secret-bearing steps.

## Revalidation

**Verdict:** true-positive

O workflow roda em pull_request e faz checkout do codigo do PR na linha 60. Antes da etapa que passa secrets.XANITIZER_LICENSE para RIGS-IT/xanitizer-action, ele executa mvn -B compile e npm install nas linhas 73-81. Em um PR same-repo, um autor nao confiavel poderia adicionar pom.xml, plugins Maven, package.json ou scripts de lifecycle npm para iniciar um processo em background. Esse processo poderia persistir ate a etapa Xanitizer e tentar ler o input de licenca no ambiente/processos do runner. Fork PRs normalmente nao recebem o secret, entao a explorabilidade principal e para branches/PRs do mesmo repositorio. Ainda assim, a sequencia atual mistura codigo de PR executado antes de uma etapa com segredo na mesma job. Nao ha separacao de jobs, cleanup rigoroso ou restricao de lifecycle scripts antes do uso do segredo.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
