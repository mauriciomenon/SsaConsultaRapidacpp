# [MEDIUM] Refs mutaveis em GitHub Actions

**File:** [`.github/workflows/powershell.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/powershell.yml#L27-L47) (lines 27, 32, 47)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

O workflow executa em push, pull_request e schedule com `security-events: write`, mas usa `actions/checkout@v4` e `github/codeql-action/upload-sarif@v3`. Como esses refs sao major tags mutaveis, um atacante que comprometa ou reprograme a tag upstream poderia executar codigo no runner com leitura do repositorio e escrita em code scanning. A action do PSScriptAnalyzer esta fixada por SHA.

## Recommendation

Fixar `actions/checkout` e `github/codeql-action/upload-sarif` por SHA completo.

## Revalidation

**Verdict:** true-positive

Li o workflow completo e ele roda em push, pull_request e schedule para master. O job declara contents: read, security-events: write e actions: read, e usa actions/checkout@v4 na linha 32 e github/codeql-action/upload-sarif@v3 na linha 47. Esses dois refs sao major tags mutaveis, enquanto a action do PSScriptAnalyzer esta fixada por SHA completo, entao a mitigacao existe apenas para uma das actions. Um atacante que comprometa ou mova uma dessas tags upstream pode executar codigo no runner e influenciar ou falsificar o SARIF enviado com a permissao de code scanning. O git diff dos workflows esta limpo e o historico mostra apenas o commit de adicao do workflow, sem patch posterior que fixe esses refs.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
