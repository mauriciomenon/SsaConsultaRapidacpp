# [HIGH_BUG] SonarCloud workflow has no checkout and empty project identifiers

**File:** [`.github/workflows/sonarcloud.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/sonarcloud.yml#L46-L67) (lines 46, 50, 58, 59, 67)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH_BUG  •  **Confidence:** high  •  **Slug:** `other-broken-security-scan`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow invokes the SonarCloud action directly, but there is no actions/checkout step, so the scanner has no checked-out source tree to analyze. The mandatory sonar.projectKey and sonar.organization values are also empty. The SONAR_TOKEN reference itself is normal, but this workflow is effectively a placeholder and will not provide meaningful security analysis.

## Recommendation

Add a pinned checkout step, set real sonar.projectKey and sonar.organization values, and validate that the scanner analyzes the intended C++/Qt sources.

## Revalidation

**Verdict:** true-positive

O workflow completo contem apenas uma etapa: `SonarSource/sonarcloud-github-action` pinada em SHA. Nao ha etapa `actions/checkout`, entao o workspace do runner nao recebe a arvore do repositorio antes do scanner. Os argumentos obrigatorios `-Dsonar.projectKey=` e `-Dsonar.organization=` estao vazios. A referencia a `SONAR_TOKEN` por secret e normal, mas o token sozinho nao torna o scanner funcional sem projeto e fonte. O `projectBaseDir: .` aponta para o diretorio atual do runner, que sem checkout nao contem o codigo do projeto. Nao encontrei `sonar-project.properties` ou outra configuracao rastreada que suprisse esses valores. O historico mostra apenas a criacao do workflow, sem correcao posterior.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
