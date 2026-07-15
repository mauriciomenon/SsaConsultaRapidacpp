# SSA Consulta Rapida 0.9.6

## Resumo

A 0.9.6 substitui o importador SSA permissivo por um contrato XLSX temporal,
nao destrutivo, escalavel e retomavel. Tambem fecha a barreira de shutdown que
ainda podia anunciar termino antes de uma arvore de processo realmente drenar.

## Importacao SSA

- Somente XLSX e importado pelo fluxo principal. XLS e inventariado como
  pendente, sem parsing ou conversao automatica; LibreOffice permanece isolado.
- Incremental faz merge seletivo; campo ausente nao apaga dado existente.
- Full rescan e all-or-nothing e preserva o banco se qualquer arquivo ou linha
  tornar o corpus invalido.
- Recencia usa data da planilha, timestamp da fonte, data do nome original e
  data de cadastro, nessa ordem. Falha de parsing bloqueia update.
- STE e SCA nao regridem. Snapshot igual pode enriquecer somente campos vazios
  e indicadores autorizados.

## Planilhas, escala e consolidacao

- O catalogo cobre 77 campos canonicos, 184 labels de origem e 168 aliases
  normalizados executaveis.
- Todas as worksheets sao lidas. Datas 1900/1904 e formulas com cache possuem
  resultado explicito.
- O rescan processa o acervo sequencialmente sem cap global de 64 e le linhas
  em blocos de 1.000. O teste de 250 mil linhas respeita 256 MiB adicionais de
  RSS.
- Lock entre processos protege discovery. Journal SQLite transacional permite
  retomar a consolidacao pos-commit sem repetir movimentos concluidos.

## Cancelamento e shutdown

- Force shutdown usa estado `Drained`, que exige zero starts pendentes e zero
  arvores ativas.
- Falha de termination bloqueia novos processos ate drain comprovado.
- O registro ocorre no sinal de start; um lider que termina imediatamente nao
  deixa o descendente invisivel para a barreira.
- `WorkflowCommandRunner` permanece em `Canceling` ate o terminal real.

## Limites declarados

- SAM permanece desabilitado por padrao. O adapter do XLSX real e a prova de
  completude alem do limite 200 ficam para 0.9.7.
- A acao atual de derivadas limpa referencias orfas; importacao explicita de
  fontes de derivadas fica para 0.9.7.
- Windows so sera declarado validado com execucao real no Windows.

## Build canonico

```bash
./scripts/make_clean
export SSA_CPP_PRESET=dev
./scripts/build-macos.sh
ctest --preset dev --output-on-failure
./scripts/package-macos.sh
```

Validacao local antes da tag: 340 de 340 testes, scanners sem achados e revisao
independente final limpa. Publicar somente em `origin` e `bitbucket`.
