# [BUG] Snyk IaC workflow scans a placeholder file that is not present

**File:** [`.github/workflows/snyk-infrastructure.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/snyk-infrastructure.yml#L36-L54) (lines 36, 39, 46, 50, 51, 52, 54)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** BUG  •  **Confidence:** high  •  **Slug:** `other-ci-misconfiguration`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow is configured with `file: your-file-to-test.yaml`, and the repository scan found no such file or obvious IaC manifests matching this job. As written, the workflow is likely to fail, produce no useful `snyk.sarif`, or upload stale/no findings, so it does not provide the intended IaC security coverage.

## Recommendation

Point Snyk at real IaC files in the repository, scan the correct directory according to Snyk's supported configuration, or remove this workflow if the project has no IaC. Add a preflight check so SARIF upload only runs when a fresh SARIF file was produced.

## Revalidation

**Verdict:** true-positive

O workflow usa explicitamente `file: your-file-to-test.yaml` na etapa Snyk IaC. Busquei os arquivos rastreados por Git e nao existe `your-file-to-test.yaml`. Tambem nao ha manifests IaC rastreados como Terraform, Helm, Kubernetes ou Compose que tornem esse alvo plausivel; os YAML rastreados sao basicamente workflows e `.pre-commit-config.yaml`. Como a etapa Snyk esta apontando para um arquivo inexistente, ela tende a falhar ou nao produzir cobertura util. A etapa seguinte tenta enviar `snyk.sarif`, mas o workflow nao garante que esse arquivo foi gerado de forma fresca. Isso quebra a finalidade declarada do workflow, que e publicar achados IaC no Code Scanning. O historico local mostra apenas a criacao do arquivo, sem patch posterior.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
