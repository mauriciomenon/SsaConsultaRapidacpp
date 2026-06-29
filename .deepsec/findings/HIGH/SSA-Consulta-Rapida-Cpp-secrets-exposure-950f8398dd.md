# [HIGH] PR-controlled background service can persist into a secret-bearing StackHawk step

**File:** [`.github/workflows/stackhawk.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/stackhawk.yml#L35-L61) (lines 35, 52, 54, 55, 57, 58, 61)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH  •  **Confidence:** medium  •  **Slug:** `secrets-exposure`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow runs on pull_request, checks out PR code, starts ./your-service.sh in the background, and then runs the StackHawk action with HAWK_API_KEY. A same-repo PR author could add or modify your-service.sh so it keeps running into the next step and inspects same-user runner processes or environment used by the action input, exposing the StackHawk API key. This is a CI secret boundary issue caused by executing repository-controlled code before a secret-consuming step in the same job.

## Recommendation

Do not run PR-controlled startup scripts in the same job that receives HAWK_API_KEY. Run StackHawk only on trusted push/schedule events, or start the service from trusted base-branch code with strict cleanup before secrets are introduced. Also set checkout persist-credentials: false where possible.

## Revalidation

**Verdict:** true-positive

O workflow roda em pull_request para master e faz checkout padrao do codigo do PR na linha 52. Logo depois executa ./your-service.sh em background na linha 55, antes da action StackHawk receber secrets.HAWK_API_KEY na linha 61. Em PRs vindos de forks, o segredo normalmente nao e exposto, mas o finding cita corretamente o caso de autor de PR no mesmo repositorio. Esse autor pode adicionar ou alterar your-service.sh para iniciar um processo persistente que aguarde a etapa seguinte e leia ambiente/processos do mesmo usuario no runner. A action StackHawk recebe o segredo como input, que costuma aparecer como variavel de ambiente INPUT_APIKEY durante a execucao da action. Nao ha cleanup, isolamento de job ou troca para codigo confiavel antes da etapa com segredo. Portanto ha um cenario concreto de exposicao do HAWK_API_KEY para PRs same-repo nao confiaveis.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
