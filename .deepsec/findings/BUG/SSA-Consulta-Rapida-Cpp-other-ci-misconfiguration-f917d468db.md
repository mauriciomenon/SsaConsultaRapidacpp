# [BUG] Rust Clippy workflow is ineffective in the current repository

**File:** [`.github/workflows/rust-clippy.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/rust-clippy.yml#L41-L52) (lines 41, 44, 45, 46, 47, 48, 49, 52)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** BUG  •  **Confidence:** high  •  **Slug:** `other-ci-misconfiguration`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The repository does not contain a `Cargo.toml` or Rust source files, but this workflow still installs Rust tooling and runs `cargo clippy`. The clippy step is marked `continue-on-error`, and the later upload expects `rust-clippy-results.sarif`; in practice this workflow is likely to fail or upload no meaningful results.

## Recommendation

Remove this workflow unless Rust code is added, or guard it with path filters and an explicit manifest check before running Cargo and uploading SARIF.

## Revalidation

**Verdict:** true-positive

Li o workflow inteiro e ele instala Rust, instala conversores SARIF via cargo install, depois executa cargo clippy --all-features --message-format=json. A busca por arquivos rastreados nao encontrou Cargo.toml, Cargo.lock ou arquivos .rs. Sem manifesto Rust, o comando cargo clippy nao tem projeto alvo no repositorio atual. O passo esta com continue-on-error: true, entao a falha pode ser mascarada e o upload posterior tenta usar rust-clippy-results.sarif, que pode estar ausente, vazio ou invalido. Isso confirma que o workflow e inefetivo para a base atual.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
