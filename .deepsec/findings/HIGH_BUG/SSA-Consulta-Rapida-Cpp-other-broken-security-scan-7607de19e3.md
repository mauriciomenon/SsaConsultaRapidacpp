# [HIGH_BUG] Snyk workflow is configured like an unadapted template and will not scan reliably

**File:** [`.github/workflows/snyk-security.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/snyk-security.yml#L50-L73) (lines 50, 52, 56, 57, 60, 61, 65, 66, 69, 70, 72, 73)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH_BUG  •  **Confidence:** high  •  **Slug:** `other-broken-security-scan`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

SNYK_TOKEN is scoped only to the Snyk setup step, but the later snyk code test, monitor, iac test, and container monitor commands run in separate steps without that environment variable. The workflow also invokes docker build and snyk container monitor with Dockerfile, but no Dockerfile exists in the repository. This makes the advertised security scan fail or run without the required authentication instead of protecting the project.

## Recommendation

Move SNYK_TOKEN to job-level env or each Snyk run step, remove template-only container steps unless a real Dockerfile is added, and verify the workflow against the C++/Qt build path.

## Revalidation

**Verdict:** true-positive

O arquivo confirma que `SNYK_TOKEN` esta definido apenas no step `Set up Snyk CLI`, nao nos steps posteriores que executam `snyk code test`, `snyk monitor`, `snyk iac test` e `snyk container monitor`. Em GitHub Actions, `env` definido em um step nao se propaga automaticamente para steps seguintes. A busca em arquivos rastreados tambem confirmou que nao ha `Dockerfile`; existe apenas `scripts/container/Containerfile.debian-build`, que nao e usado pelo comando `docker build -t your/image-to-test .` nem por `--file=Dockerfile`. Assim, a parte de container falha no estado atual da branch base. O comando `snyk code test --sarif > snyk-code.sarif` tambem pode falhar antes do upload, e as etapas monitor/report exigem autenticacao. Isso torna o workflow de seguranca operacionalmente quebrado, em vez de fornecer cobertura confiavel. Nao ha patch posterior no historico local.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
