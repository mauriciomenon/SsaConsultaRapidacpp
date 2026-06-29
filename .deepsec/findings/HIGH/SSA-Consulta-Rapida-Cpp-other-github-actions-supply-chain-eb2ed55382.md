# [HIGH] Secret-bearing pull_request_target workflow trusts mutable third-party actions

**File:** [`.github/workflows/crda.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/crda.yml#L67-L124) (lines 67, 68, 69, 76, 78, 103, 106, 108, 122, 124)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow runs on pull_request_target for PRs to master, which gives the job access to base-repository secrets and token context. It then passes github.token to redhat-actions/openshift-tools-installer@v1 and CRDA_KEY to redhat-actions/crda@v1, and installs crda: latest. The default checkout means this is not direct PR-head code execution, but a compromised or retagged third-party action or latest CRDA CLI release can be triggered by an attacker opening or updating a PR and can exfiltrate CRDA_KEY or abuse the GitHub token/security-events write permission.

## Recommendation

Pin every action to a full commit SHA, pin the CRDA CLI to an immutable version/checksum, and avoid exposing CRDA_KEY or github.token to PR-triggered jobs. Prefer running secret-dependent scans on push, schedule, workflow_dispatch, or a label-gated trusted workflow.

## Revalidation

**Verdict:** true-positive

O workflow ainda usa pull_request_target para PRs contra master nas linhas 67-69, entao roda no contexto do repositorio base e pode receber secrets do repositorio. A job concede security-events: write e passa github.token ao redhat-actions/openshift-tools-installer@v1, alem de passar secrets.CRDA_KEY ao redhat-actions/crda@v1. As refs actions/checkout@v4, redhat-actions/openshift-tools-installer@v1 e redhat-actions/crda@v1 sao tags mutaveis, e o CLI e instalado como crda: latest. Um atacante que comprometa ou mova uma dessas tags, ou o artefato latest do CLI, pode executar codigo no job quando qualquer PR acionar o workflow. Esse codigo teria caminho claro para ler o input crda_key ou abusar do token com security-events: write. O checkout padrao reduz risco de execucao direta de codigo do PR, mas nao mitiga a execucao de codigo mutavel de terceiros no job com segredo. O historico mostra que o arquivo foi adicionado por e98a155 e nao ha commit posterior removendo essas linhas.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
