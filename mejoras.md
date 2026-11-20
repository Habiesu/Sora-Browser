# Mejoras pendientes

Cosas vistas al compilar y medir el Hito 1 en Windows (20/11/2026). No son urgentes; ninguna
bloquea el Hito 1. Ordenadas de más a menos útil.

## Medición (`bench/bench.ps1`)

- **El "arranque" mide cuándo existe la ventana, no cuándo se ve la página.** `MainWindowHandle`
  aparece en cuanto se crea la ventana, aunque aún esté en blanco. Sora ya tiene `--bench` con
  `first_load_ms`; falta una medida equivalente en Chrome (p. ej. `--enable-logging` + evento de
  carga, o comparar con capturas) para comparar tiempo hasta página cargada.
- **Se mata el navegador a la fuerza** (`Stop-Process -Force`) al final de cada ejecución. Si Sora
  está escribiendo en su perfil en ese momento podría corromperlo; mejor cerrar la ventana
  (`CloseMainWindow()`) y forzar solo si no sale en unos segundos.
- **Sora con perfil real y Chrome con perfil temporal reutilizado.** Es lo que dice el script, pero
  conviene dejarlo escrito en cada resultado; si Sora acumula caché, la comparación cambia con el
  tiempo.
- **Si Sora ya está abierto**, la medición puede dar datos falsos (la nueva instancia podría pasar
  la URL a la ventana existente). El script debería avisar y parar si hay `sora.exe` o
  `chrome.exe` con el perfil de prueba en marcha.

## Compilación en Windows (`scripts/build-windows.ps1`)

- **Aviso `"vswhere.exe" no se reconoce`** al cargar `vcvars64.bat`: es inofensivo pero ensucia el
  log. Se quita añadiendo la carpeta de `vswhere.exe` al `PATH` antes de llamar a `vcvars64.bat`.
- **No comprueba la versión de Inno Setup** (CLAUDE.md pide 6.3+); solo que exista `ISCC.exe`.
- **`fetch-qt.py` descarga cada archivo entero en memoria** (WebEngine pesa cientos de MB).
  Funciona, pero en un PC con poca RAM convendría guardar en disco y descomprimir desde ahí.
- **Qt publica WebEngine recompilado en subcarpetas numeradas** (`6120/61400/`). `fetch-qt.py`
  elige la más reciente, pero si Qt vuelve a publicar un qtbase nuevo sin recompilar WebEngine,
  las pruebas fallarán con `0xc0000139`. Si pasa, la causa es esa.

## Repositorio

- **Falta `.gitattributes`**: Git avisa de cambios LF↔CRLF en Windows. Con
  `* text=auto eol=lf` (y `*.ps1 text eol=crlf`, `*.iss text eol=crlf`) se evita.
