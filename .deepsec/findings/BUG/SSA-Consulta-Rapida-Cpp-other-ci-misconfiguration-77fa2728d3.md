# [BUG] Workflow aponta para requirements.txt inexistente

**File:** [`.github/workflows/pyre.yml`](https://github.com/mauriciomenon/SSA_Consulta_Rapida_Cpp/blob/master/.github/workflows/pyre.yml#L46) (lines 46)
**Project:** SSA_Consulta_Rapida_Cpp
**Severity:** BUG  •  **Confidence:** medium  •  **Slug:** `other-ci-misconfiguration`

## Owners

**Suggested assignee:** `54405514+mauriciomenon@users.noreply.github.com` _(via last-committer)_

## Finding

A action Pyre recebe `requirements-path: 'requirements.txt'`, mas `requirements.txt` nao existe no repositorio e nao ha manifesto Python equivalente rastreado. Isso tende a quebrar ou invalidar a analise do workflow.

## Recommendation

Adicionar o manifesto esperado ou remover/ajustar `requirements-path` para um arquivo real.

## Revalidation

**Verdict:** true-positive

O workflow passa requirements-path: 'requirements.txt' para facebook/pyre-action. A busca por arquivos rastreados nao encontrou requirements.txt, requirements*.txt, pyproject.toml, setup.py, setup.cfg, Pipfile, poetry.lock ou tox.ini. A busca de arquivos Python tambem nao encontrou fontes Python rastreadas no repositorio atual. Portanto o input aponta para um arquivo inexistente e o workflow nao esta alinhado ao conteudo real do projeto C++/Qt/CMake. Mesmo que a action tolere a ausencia do arquivo em algum modo, a configuracao tende a falhar ou produzir analise sem cobertura util.

## Recent committers (`git log`)

- Maurício Menon <54405514+mauriciomenon@users.noreply.github.com> (2026-05-12)
