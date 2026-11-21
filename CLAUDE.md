# Sora — guía para Claude

Sora es un navegador web de escritorio para uso personal del autor. Prioridades, en este orden:
**rendimiento**, **privacidad** y **seguridad**. Lo que debe distinguirlo de otros navegadores es
el **rendimiento**: arrancar y responder al menos tan rápido como Chrome y gastar menos memoria.

## Dónde está todo

- **Diseño aprobado (manda sobre todo lo demás):** `docs/superpowers/specs/2026-11-18-sora-browser-design.md`
- **Planes por hito:** `docs/superpowers/plans/` (Hito 1: `2026-11-19-hito1-prototipo.md`)
- **Aspecto visual "Niebla":** sección E de https://claude.ai/artifact/JcdkoVfAPYLPF1npdeXcAa
  (pestañas en la barra de título, minimizar/maximizar/cerrar **a la derecha**, barra de herramientas
  con dirección redondeada, escudo de bloqueos, estrella y menú, barra de favoritos, colores planos
  gris azulado, tema claro y oscuro).
- **Mediciones:** `bench/` (`bench.ps1` compara con Chrome; resultados en `bench/results/`).

## Decisiones cerradas (no cambiarlas sin preguntar al usuario)

- Motor: **Qt 6.12 + Qt WebEngine**, en **C++20** con **Qt Widgets** (nada de QML, HTML, Electron ni Tauri).
- Bloqueador: **adblock-rust de Brave**, en el crate `adblock-ffi/` (API C), enlazado con Corrosion.
- Plataformas: **Windows 10/11 x64 primero**, Linux también. Android, más adelante y como app aparte.
- **Instalador con autoactualización** (Inno Setup, por usuario, sin administrador); comprobación de
  versión nueva **una vez al día** en GitHub Releases.
- Privacidad: **sin telemetría**; buscador por defecto **DuckDuckGo**; cookies de terceros bloqueadas;
  modo solo HTTPS; Safe Browsing mediante la Update API v4 (comprobación local).
- Netflix/Crunchyroll (DRM Widevine): se intentará en un experimento; si falla, botón "Abrir en Edge".
- Extensiones de Chrome: después de la versión 1 (Qt 6.10+ las soporta de forma experimental).

## Compilar y probar

Linux:
```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64
cmake --build build
ctest --test-dir build --output-on-failure          # usa QT_QPA_PLATFORM=offscreen
cargo test --manifest-path adblock-ffi/Cargo.toml
```
Como root o en contenedores, Qt WebEngine necesita `QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox` (solo ahí).

Windows (PowerShell): `scripts\build-windows.ps1` compila, pasa las pruebas y deja el instalador en `dist\`.
Necesita Visual Studio 2022 Build Tools (C++), CMake, Ninja, Rust (MSVC), Python 3, Git e Inno Setup 6.3+.
Qt se descarga solo en `.qt\`.

Medir contra Chrome: `powershell -ExecutionPolicy Bypass -File bench\bench.ps1 -Sora <ruta a sora.exe> -Runs 10`.
`sora --bench <url>` imprime `{"window_shown_ms":N,"first_load_ms":N}` y sale.

## Normas de trabajo

- Textos de la app y mensajes al usuario **en español**.
- **Pruebas primero** (Qt Test en `tests/`, `cargo test` en `adblock-ffi/`): ver fallar, luego implementar.
- Nada debe frenar el arranque: lo pesado (listas, motor) va en segundo plano después de mostrar la ventana.
- Rama de trabajo: **`ccr-765bf7fa-sutn5o`**. No subir `build*/`, `.qt/`, `dist/` ni `.superpowers/`.
- No hay GitHub Actions (sin minutos): las pruebas en Windows se hacen en el PC del usuario.
- Cada hito nuevo se planifica antes de programarlo (`docs/superpowers/plans/`).
- Respóndeme directo.
- No me expliques lo que no he preguntado.

## Estado

- **Hito 1 (prototipo medible):** hecho. Compila y pasa las pruebas en Windows; medido en
  `bench/results/2026-11-20-hito1.md`.
- **Hito 2 (interfaz Niebla):** hecho (plan `docs/superpowers/plans/2026-11-20-hito2-niebla.md`,
  capturas en `hito2-capturas/`). Arranque y memoria siguen ≤ Chrome
  (`bench/results/2026-11-20-hito2.md`). Pendiente de probar a mano por el usuario: Snap Layouts al
  pasar sobre maximizar, arrastrar y redimensionar con el ratón.
  Si Sora deja de igualar a Chrome en arranque o memoria con una pestaña, parar y hablarlo con el usuario.
- **Hito 3 (paridad con Chrome):** hecho (plan `docs/superpowers/plans/2026-11-21-hito3-paridad-chrome.md`).
  Historial, marcadores, descargas, ajustes, extensiones, menú principal, ventanas e incógnito,
  predicción en la barra y anuncios de YouTube. Las páginas internas son HTML en `sora://` con
  QWebChannel (`InternalApi`); el marco de la ventana sigue en Qt Widgets. Arranque y memoria ≤ Chrome
  (`bench/results/2026-11-21-hito3.md`). Pendiente: restaurar la sesión al abrir.
- **Siguientes:** 4 Privacidad (escudo,
  HTTPS, cookies, Safe Browsing, listas diarias) · 5 Pestañas dormidas · 6 Descargas, modo privado,
  ajustes · 7 Instalador con autoactualización · Experimento DRM.
