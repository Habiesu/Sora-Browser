# Mediciones de rendimiento

`bench.ps1` compara Sora con Chrome en un PC con Windows: tiempo de arranque hasta que aparece
la ventana y memoria total (todos los procesos) tras abrir una página.

```powershell
powershell -ExecutionPolicy Bypass -File bench\bench.ps1 -Sora "C:\ruta\a\sora.exe" -Runs 10
```

Opciones: `-Chrome <ruta>` (por defecto se busca solo), `-Url <página>`,
`-SettleSeconds <s>` (espera antes de medir memoria, 15 por defecto).

Sora también acepta `--bench <url>`: imprime `{"window_shown_ms":N,"first_load_ms":N}` medido
desde el inicio de `main()` y se cierra.

Los resultados se guardan en `bench/results/`.
