# [HIGH] Release jobs run mutable third-party code with repository write privileges

**File:** [`.github/workflows/release.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/release.yml#L14-L221) (lines 14, 15, 32, 49, 79, 81, 84, 90, 104, 127, 128, 141, 160, 164, 180, 192, 208, 221)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** HIGH  •  **Confidence:** high  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow grants `contents: write` globally at lines 14-15, so every build job receives release-capable repository permissions even though only the publish job needs them. Those jobs then run multiple mutable action refs and a mutable container image, including `actions/checkout@v4`, `ilammy/msvc-dev-shell@v1`, `jurplel/install-qt-action@v4`, `actions/upload-artifact@v4`, `actions/download-artifact@v4`, `softprops/action-gh-release@v2`, and `archlinux:latest`. A compromised action tag, moved major tag, or compromised container image in any build job could tamper with release artifacts or use persisted checkout credentials/token privileges to modify repository contents or releases.

## Recommendation

Pin every action to a full commit SHA and pin the Arch container by digest. Move `contents: write` to the `publish` job only, set build jobs to `contents: read`, and use `persist-credentials: false` on checkout steps that do not need to push.

## Revalidation

**Verdict:** true-positive

O workflow ainda define permissions: contents: write globalmente nas linhas 14-15, antes de todas as jobs. Isso faz com que jobs de build tambem recebam permissao de escrita em conteudo, embora somente publish precise criar release. As jobs executam varias refs mutaveis: actions/checkout@v4, ilammy/msvc-dev-shell@v1, jurplel/install-qt-action@v4, actions/cache@v4, actions/upload-artifact@v4, actions/download-artifact@v4 e softprops/action-gh-release@v2. A job Arch tambem usa container: archlinux:latest, que e uma imagem mutavel. Um comprometimento ou retag dessas dependencias durante um push de tag ou workflow_dispatch poderia alterar artefatos, publicar conteudo indevido ou usar credenciais persistidas pelo checkout. O risco e real de supply chain porque o codigo externo roda antes da publicacao e com permissoes superiores ao necessario para os builds. Os commits posteriores no historico de release.yml nao removeram o contents: write global nem fixaram actions por SHA/digest.

## Recent committers (`git log`)

- Mauricio Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-06-26)
