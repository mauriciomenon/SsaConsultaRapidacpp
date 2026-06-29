# [MEDIUM] Rust scanning workflow trusts mutable actions and latest Cargo packages

**File:** [`.github/workflows/rust-clippy.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/rust-clippy.yml#L27-L52) (lines 27, 31, 41, 42, 52)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** MEDIUM  •  **Confidence:** medium  •  **Slug:** `other-github-actions-supply-chain`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

The workflow uses mutable action refs for checkout and SARIF upload, and installs `clippy-sarif` and `sarif-fmt` without pinning versions. If one of those action tags or crates is compromised, the attacker can control the generated SARIF that is later uploaded with `security-events: write`, poisoning code scanning results or hiding findings.

## Recommendation

Pin GitHub actions to full commit SHAs. Pin Cargo package versions and use locked/reproducible installs, or vendor the SARIF conversion tooling.

## Revalidation

**Verdict:** true-positive

O workflow usa actions/checkout@v4 e github/codeql-action/upload-sarif@v3 por tags mutaveis, enquanto actions-rs/toolchain esta fixada por SHA. Alem disso, o passo cargo install clippy-sarif sarif-fmt nao fixa versoes nem usa lockfile reproduzivel. Esse passo executa antes do cargo clippy, entao o risco de supply chain dos crates existe mesmo que o repositorio nao tenha Cargo.toml. Um comprometimento de tag de action ou de crate instalado no momento do CI pode executar codigo no runner e gerar SARIF falso que depois e enviado com security-events: write. Nao ha diff local ou commit posterior removendo essa configuracao.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
