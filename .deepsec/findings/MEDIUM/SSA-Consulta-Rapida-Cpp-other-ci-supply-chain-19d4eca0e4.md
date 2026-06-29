# [MEDIUM] Mutable SARIF upload action can execute changed upstream code

**File:** [`.github/workflows/apisec-scan.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/apisec-scan.yml#L51-L69) (lines 51, 52, 53, 69)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-ci-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow pins the APIsec action to a full commit SHA, but the SARIF upload step uses github/codeql-action/upload-sarif@v3. Major-version tags are mutable. If that upstream ref is moved or compromised, attacker-controlled action code would run in this job with security-events: write and actions: read, allowing tampering with code-scanning results. The APIsec username/password references are normal secret inputs and are not hardcoded secrets.

## Recommendation

Pin github/codeql-action/upload-sarif to a reviewed full commit SHA and update it through a controlled dependency update process.

## Revalidation

**Verdict:** true-positive

O workflow fixa apisec-inc/apisec-run-scan por SHA completo na linha 58, mas ainda usa github/codeql-action/upload-sarif@v3 na linha 69. Essa ref v3 e uma tag mutavel. O job concede security-events: write e actions: read, exatamente as permissoes usadas pelo upload de SARIF. Se a tag upstream fosse movida ou comprometida, codigo alterado poderia rodar no job e enviar, suprimir ou adulterar resultados de code scanning. O finding nao depende de secrets hardcoded; as credenciais APIsec sao inputs de secrets e nao aparecem em texto claro no arquivo. Nao ha pin por SHA para o upload-sarif no estado atual.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
