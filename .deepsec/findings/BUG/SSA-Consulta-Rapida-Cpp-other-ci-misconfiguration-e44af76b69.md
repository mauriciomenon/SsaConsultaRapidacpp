# [BUG] Workflow aponta para requirements.txt inexistente

**File:** [`.github/workflows/pysa.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/pysa.yml#L48) (lines 48)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** BUG  •  **Confidence:** medium  •  **Slug:** `other-ci-misconfiguration`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

A action Pysa recebe `requirements-path: 'requirements.txt'`, mas `requirements.txt` nao existe no repositorio e nao ha manifesto Python equivalente rastreado. Isso tende a quebrar ou invalidar a analise do workflow.

## Recommendation

Adicionar o manifesto esperado ou remover/ajustar `requirements-path` para um arquivo real.

## Revalidation

**Verdict:** true-positive

O workflow Pysa define requirements-path: 'requirements.txt' na configuracao da action. O indice do Git nao contem requirements.txt nem outro manifesto Python usual, como pyproject.toml, setup.py, setup.cfg, Pipfile ou poetry.lock. A varredura de fontes tambem nao encontrou arquivos .py rastreados, apenas scripts PowerShell em outra busca de scripts. Assim, a configuracao atual referencia um arquivo ausente e nao corresponde ao stack principal do repositorio. Isso torna a analise Pysa propensa a falhar ou a entregar resultado sem cobertura real.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
