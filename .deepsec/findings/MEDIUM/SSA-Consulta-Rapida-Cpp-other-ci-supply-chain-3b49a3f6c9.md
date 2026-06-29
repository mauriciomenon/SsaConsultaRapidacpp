# [MEDIUM] Mutable actions run around a Codacy scan job

**File:** [`.github/workflows/codacy.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/codacy.yml#L30-L59) (lines 30, 31, 32, 33, 39, 47, 59)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-ci-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The Codacy action is pinned to a commit SHA, and the project-token secret is an intended input rather than a hardcoded secret. However, the same job uses actions/checkout@v4 and github/codeql-action/upload-sarif@v3 by mutable major tags. If either upstream ref is moved or compromised, attacker code could run in the job and abuse the available GitHub token permissions, including security-events: write.

## Recommendation

Pin actions/checkout and github/codeql-action/upload-sarif to reviewed full commit SHAs. Keep the Codacy token scoped to the minimum required permissions in Codacy.

## Revalidation

**Verdict:** true-positive

O workflow atual tem permissions explicitas com contents: read no topo e, no job, contents: read, security-events: write e actions: read. A action Codacy esta pinada por SHA completo, mas o checkout usa actions/checkout@v4 e o upload SARIF usa github/codeql-action/upload-sarif@v3. Esses dois refs continuam mutaveis e executam no mesmo job que possui permissao security-events: write. Um ataque concreto seria comprometer o ref upload-sarif@v3 para adulterar ou suprimir resultados SARIF antes do envio, ou abusar do GITHUB_TOKEN do job para gravar eventos de seguranca falsos. O checkout comprometido tambem executaria antes da analise e receberia o token usado pelo checkout. O segredo CODACY_PROJECT_TOKEN e um input intencional da action pinada, mas isso nao mitiga o risco dos refs mutaveis ao redor. Nao encontrei alteracao posterior que pinasse esses refs.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
