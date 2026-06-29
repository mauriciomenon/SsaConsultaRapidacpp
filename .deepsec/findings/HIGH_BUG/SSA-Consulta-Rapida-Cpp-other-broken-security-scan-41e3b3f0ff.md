# [HIGH_BUG] SonarQube workflow has no checkout and an empty project key

**File:** [`.github/workflows/sonarqube.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/sonarqube.yml#L44-L58) (lines 44, 48, 58)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH_BUG  •  **Confidence:** high  •  **Slug:** `other-broken-security-scan`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow runs the pinned SonarQube scan action without checking out the repository first, and the mandatory -Dsonar.projectKey argument is empty. The secret references are expected for SonarQube, but the scan configuration is incomplete and will fail or analyze an empty workspace.

## Recommendation

Add a pinned checkout step, configure a real sonar.projectKey and SONAR_HOST_URL, and verify the workflow on the actual repository layout.

## Revalidation

**Verdict:** true-positive

O workflow SonarQube tambem chama diretamente `SonarSource/sonarqube-scan-action` pinada em SHA sem uma etapa anterior de checkout. Isso deixa o scanner sem a arvore de fontes do repositorio para analisar. O argumento obrigatorio `-Dsonar.projectKey=` esta vazio no YAML atual. `SONAR_TOKEN` e `SONAR_HOST_URL` sao referenciados como secrets, o que e esperado, mas nao corrige a ausencia de checkout nem a chave de projeto vazia. Nao ha arquivo de configuracao Sonar rastreado que preencha esses dados fora do workflow. Portanto a acao deve falhar ou rodar sem analise util do projeto real. O historico local indica que esse arquivo tambem nao recebeu patch depois de criado.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
