# [MEDIUM] Checkout usa ref mutavel

**File:** [`.github/workflows/pyre.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/pyre.yml#L33-L36) (lines 33, 36)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

O job concede `security-events: write` e usa `actions/checkout@v4`. Esse major tag e mutavel; se o ref upstream for comprometido ou movido, codigo nao revisado pode rodar antes da action Pyre fixada por SHA, com acesso ao token do job conforme permissoes declaradas.

## Recommendation

Fixar `actions/checkout` por SHA completo.

## Revalidation

**Verdict:** true-positive

O workflow Pyre concede actions: read, contents: read e security-events: write no job. O primeiro passo usa actions/checkout@v4 com submodules: true, antes da action facebook/pyre-action, que esta fixada por SHA completo. Como @v4 e um major tag movel, uma alteracao maliciosa no ref upstream executaria antes da analise Pyre e com o token do job disponivel conforme as permissoes declaradas. GitHub Actions nao torna esses major tags imutaveis por contrato do workflow, entao a protecao esperada seria pinning por SHA. Nao ha diff local nem commit posterior no historico corrigindo esse uso.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
