# Autoracao de temas e relevo glossy

## O que produz o efeito de `ssa-dark`

Nao existe `Gradient` nem sombra global. O efeito metalizado percebido vem de
uma escada de luminosidade entre superficies, bordas visiveis e botoes quiet:

| Papel | `ssa-dark` |
| --- | --- |
| `window` | `#28303c` |
| `surface` | `#2d3743` |
| `panel` | `#323b48` |
| `panelRaised` | `#3d4857` |
| `borderSoft` | `#485669` |
| `border` | `#596779` |
| `rowAlt` | `#2f3945` |
| `rowSelected` | `#475b62` |
| `accent` | `#86aeb8` |
| `accentStrong` | `#9fc1c8` |
| `accentSoft` | `#b4cbd0` |

`ActionButton` usa `panelRaised` como fundo normal, `border` como contorno,
`accentSoft` no hover, `accent` pressionado e `accentStrong` no texto quando
`quietAccent` esta ativo. Hoje esse tratamento e ligado por nome para
`ssa-dark` e `gruvbox`.

## Como replicar sem copiar cegamente

1. Mantenha a ordem luminosa `window < surface < panel < panelRaised` em tema
   escuro. Em tema claro, preserve separacao perceptivel equivalente.
2. Coloque `borderSoft` entre painel e `border`; a borda nao deve desaparecer
   nem dominar o controle.
3. Use `accentSoft` como reflexo de hover e `accent` como estado pressionado.
4. Garanta foreground AA para normal, hover, pressed, disabled e selecionado.
5. Prefira um token semantico futuro, como `quietAccentControls`, em vez de
   acrescentar novos nomes ao hardcode. Essa mudanca exige slice proprio.
6. Rode Theme Lab, `ssa_qml_theme_gallery_tests`, smokes 1580/1180 e captura
   real Retina antes de publicar a paleta.

Nao altere larguras, padding ou tipografia para obter o efeito. O relevo e uma
relacao de cores. A perda observada em 20/07 nao foi remocao do `ssa-dark`: a
preferencia runtime havia mudado para `classico`, possivelmente pelo botao
circular ao lado de Preferencias, que troca e persiste o tema com um clique.
