# Optimización 1 — Sora contra sí mismo (21/11/2026)

Sin comparar con Chrome (pedido por el usuario). Dos versiones instaladas, 10 arranques intercalados
con `sora --bench https://es.wikipedia.org/wiki/Navegador_web`, Sora en la gráfica NVIDIA.

| Versión | Arranque (mediana) | Primera carga (mediana) |
|---|---|---|
| Antes (`33b2941`) | 502 ms | 1029 ms |
| Después (`e496752`) | **391 ms** | 1049 ms (igual, ruido) |

Cambios: la ventana se pinta antes de crear la vista web de la primera pestaña, y la caché del
bloqueador se valida por tamaño y fecha de las listas en vez de leer y resumir ~4 MB en cada arranque.

Fases del arranque (`SORA_TRACE=1`): ~30 ms Qt, **~265 ms crear el perfil web** (arranca Chromium y
la GPU), ~105 ms ventana, ~75 ms primera pestaña.

Probado y descartado: pintar la ventana antes de crear el perfil. Sale ~110 ms antes, pero la primera
carga empeora ~400 ms (Qt rehace la superficie de la ventana para la GPU al llegar la vista web).
