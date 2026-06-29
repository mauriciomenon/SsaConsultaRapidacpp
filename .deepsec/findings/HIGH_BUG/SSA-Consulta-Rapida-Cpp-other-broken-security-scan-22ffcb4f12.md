# [HIGH_BUG] StackHawk workflow still contains placeholder service setup

**File:** [`.github/workflows/stackhawk.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/stackhawk.yml#L54-L61) (lines 54, 55, 57, 59, 61)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH_BUG  •  **Confidence:** high  •  **Slug:** `other-broken-security-scan`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow runs ./your-service.sh, but no such file exists in the repository, and no stackhawk.yml or stackhawk.yaml configuration file was found. The HawkScan step also has continue-on-error: true, so even scan failures would not fail the workflow. As written, this DAST job is not a reliable security control.

## Recommendation

Replace the placeholder service command with the real application startup, add the required StackHawk config, and set continue-on-error according to the intended blocking policy.

## Revalidation

**Verdict:** true-positive

O arquivo ainda contem o comando placeholder ./your-service.sh & na linha 55 e o proprio comentario pede atualizacao. A busca por arquivos do repositorio nao encontrou your-service.sh, stackhawk.yml ou stackhawk.yaml. Sem o servico real e sem configuracao StackHawk, a etapa HawkScan nao tem uma aplicacao/configuracao confiavel para analisar. Alem disso, continue-on-error: true na linha 59 impede que falhas da varredura sejam necessariamente bloqueantes. Mesmo que o comando em background nao interrompa o step, o controle de DAST fica inoperante ou nao confiavel. O historico mostra apenas a adicao inicial do workflow, sem adaptacao posterior ao projeto.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
