# [MEDIUM] CodeQL workflow uses mutable action refs

**File:** [`.github/workflows/codeql.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/codeql.yml#L15-L37) (lines 15, 16, 17, 18, 19, 21, 22, 30, 37)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-ci-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The CodeQL workflow runs actions/checkout@v4, jurplel/install-qt-action@v4, github/codeql-action/init@v4, and github/codeql-action/analyze@v4 by mutable major tags. The job permissions are mostly least-privilege, but a compromised or moved action ref could still execute arbitrary code in the runner and tamper with analysis output or use the job's security-events permission.

## Recommendation

Pin all actions in the CodeQL workflow to full commit SHAs and update them through reviewed dependency updates.

## Revalidation

**Verdict:** true-positive

O workflow atual usa actions/checkout@v4, jurplel/install-qt-action@v4, github/codeql-action/init@v4 e github/codeql-action/analyze@v4. Todos sao refs mutaveis por major version, nao SHAs completos. O job tem permissions explicitas com security-events: write, packages: read, actions: read e contents: read. Um ataque concreto seria comprometer uma dessas tags, especialmente install-qt-action ou codeql-action, para executar codigo no runner e adulterar a analise ou o upload de resultados. Como o workflow tambem executa configure e build, uma action comprometida teria ponto de execucao antes e depois da compilacao. A permissao contents: read reduz impacto em escrita de codigo, mas security-events: write continua relevante para integridade de code scanning. O historico recente apenas alinhou Qt e nao corrigiu pinning de actions.

## Recent committers (`git log`)

- Mauricio Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-06-26)
