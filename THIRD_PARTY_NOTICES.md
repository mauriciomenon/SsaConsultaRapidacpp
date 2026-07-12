# Third-party notices

## Tinted Theming schemes

The native themes listed below include modified palette data derived from the
Tinted Theming Base16 scheme catalog. The catalog revision is pinned to commit
`2ccef2f4b22e3cab5a9292811f7133a07eeba4a7` and is distributed under the MIT
license in `third_party/tinted-themes/LICENSE`.

The application does not include or execute Tinted Builder. That GPL-3.0-only
tool is used externally during catalog maintenance and is documented in
`tools/theme_lab/README.md`.

| Shipped theme | Scheme author | Original inspiration |
| --- | --- | --- |
| ayu-light, ayu-mirage | Tinted Theming and Ayu Theme | https://github.com/ayu-theme/ayu-colors |
| flexoki-dark, flexoki-light | Steph Ango | https://github.com/kepano/flexoki |
| kanagawa, kanagawa-dragon | Tommaso Laurenzi and Stefan Weigl-Bosker | https://github.com/rebelot/kanagawa.nvim |
| rose-pine, rose-pine-moon, rose-pine-dawn | Emilia Dunfelt | https://github.com/rose-pine/rose-pine-theme |
| primer-dark, primer-light | Jimmy Lin | https://primer.style/ |
| oxocarbon-light | shaunsingh, IBM, and Tinted Theming | https://github.com/nyoom-engineering/oxocarbon.nvim |

The Base16 values were transformed into application-specific semantic roles,
adjusted for WCAG AA text contrast, bounded saturation, surface separation,
and stable 8-bit RGB output. The exact input hashes are recorded in
`third_party/tinted-themes/LOCK.json`.
