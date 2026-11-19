# Hito 1 — Prototipo medible: plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Un ejecutable `sora` mínimo (una ventana Qt WebEngine que navega, con el bloqueador de Brave
interceptando peticiones) que compila en Linux y Windows y que se puede medir contra Chrome en el PC
del usuario.

**Architecture:** Aplicación C++20/Qt Widgets. El bloqueo vive en un crate Rust (`adblock-ffi`) que
envuelve `adblock` (Brave) con una API C, enlazado como biblioteca estática vía Corrosion. Un
`QWebEngineUrlRequestInterceptor` consulta el motor en cada petición. Un script de PowerShell mide
arranque y memoria de Sora y de Chrome de forma simétrica.

**Tech Stack:** Qt 6.12.0 (Widgets, WebEngineWidgets, Network, Test), CMake ≥ 3.24, Ninja, Rust
estable, crate `adblock` 0.13.x, Corrosion v0.5, GitHub Actions (ubuntu-24.04, windows-2022/MSVC).

**Spec:** `docs/superpowers/specs/2026-11-18-sora-browser-design.md` (§6 "Flujo de una petición",
§7 "Rendimiento", §8 "Calidad y entrega").

## Global Constraints
- C++20, Qt Widgets (no QML), sin interfaz en HTML.
- Qt: última versión estable 6.x con Qt WebEngine publicada → **6.12.0**.
- Sin telemetría ni conexiones de fondo no documentadas. En este hito la única conexión propia es la
  descarga de listas de filtros cuando no existen en disco.
- El arranque nunca espera al bloqueador: la ventana se muestra antes de que el motor esté listo
  (mientras no esté listo, no se bloquea nada).
- Coste del bloqueador: < 50 µs de media por petición.
- Listas de filtros: EasyList `https://easylist.to/easylist/easylist.txt`,
  EasyPrivacy `https://easylist.to/easylist/easyprivacy.txt`,
  uBlock filters `https://ublockorigin.github.io/uAssets/filters/filters.txt`,
  uBlock badware `https://ublockorigin.github.io/uAssets/filters/badware.txt`.
- Mensajes visibles al usuario en español.

## Review Focus
1. **Motor aún cargando** cuando llegan las primeras peticiones → deben pasar sin bloquearse y sin
   bloqueos de hilo (test en Tarea 4).
2. **Caché del motor corrupta o de otra versión** del crate → se descarta y se recompila desde las
   listas, sin cerrar la aplicación (test en Tarea 5).
3. **URLs no http(s)** (`data:`, `blob:`, `chrome-extension:`, `about:`, `file:`) → nunca se bloquean
   ni hacen fallar `Request::new` (test en Tarea 2).
4. **Sin red en el primer arranque** (no hay listas) → el navegador funciona sin bloqueo y lo
   reintenta en el siguiente arranque (test en Tarea 5).
5. **Peticiones desde varios hilos a la vez** → `check` es seguro entre hilos (test en Tarea 3).

---

## Estructura de archivos

```
CMakeLists.txt                      raíz: proyecto, Qt, Corrosion, subdirectorios
adblock-ffi/Cargo.toml              crate staticlib "sora_adblock"
adblock-ffi/src/lib.rs              API C
adblock-ffi/include/sora_adblock.h  cabecera C
src/CMakeLists.txt                  biblioteca sora_core + ejecutable sora
src/app/main.cpp                    arranque, medición de tiempos
src/app/MainWindow.{h,cpp}          ventana mínima: barra de dirección + QWebEngineView
src/privacy/AdblockEngine.{h,cpp}   envoltorio RAII C++ sobre la API C
src/privacy/RequestInterceptor.{h,cpp}
src/privacy/FilterListStore.{h,cpp} descarga/caché de listas y del motor serializado
tests/CMakeLists.txt
tests/tst_adblockengine.cpp
tests/tst_requestinterceptor.cpp
tests/tst_filterliststore.cpp
tests/data/rules-small.txt
bench/bench.ps1                     medición Sora vs Chrome en Windows
bench/README.md
.github/workflows/ci.yml
```

---

### Tarea 1: Esqueleto CMake y ventana que navega

**Files:**
- Create: `CMakeLists.txt`, `src/CMakeLists.txt`, `src/app/main.cpp`, `src/app/MainWindow.h`,
  `src/app/MainWindow.cpp`, `tests/CMakeLists.txt`, `tests/tst_mainwindow.cpp`, `.gitignore`

**Interfaces:**
- Produces: biblioteca estática CMake `sora_core` (todo `src/` salvo `main.cpp`) y ejecutable `sora`;
  `class MainWindow : public QMainWindow { explicit MainWindow(QWebEngineProfile*, QWidget* = nullptr);
  void navigate(const QString& input); QWebEngineView* view() const; }`;
  función libre `QUrl sora::urlFromInput(const QString& input)` en `MainWindow.h`.

- [ ] **Step 1: Test que falla** — `tests/tst_mainwindow.cpp` (Qt Test, `QTEST_MAIN`):
  ```cpp
  QCOMPARE(sora::urlFromInput("example.com"), QUrl("https://example.com"));
  QCOMPARE(sora::urlFromInput("http://a.b/c"), QUrl("http://a.b/c"));
  QCOMPARE(sora::urlFromInput("hola mundo").host(), QString("duckduckgo.com"));
  QCOMPARE(QUrlQuery(sora::urlFromInput("hola mundo")).queryItemValue("q"), QString("hola mundo"));
  QCOMPARE(sora::urlFromInput("localhost:8080"), QUrl("http://localhost:8080"));
  ```
- [ ] **Step 2:** `cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/opt/qt/6.12.0/gcc_64 && cmake --build build`
  → falla al compilar (`urlFromInput` no existe).
- [ ] **Step 3:** Implementar `urlFromInput` (con esquema → tal cual; parece dominio o `localhost[:puerto]`
  → https, salvo localhost que usa http; si no → `https://duckduckgo.com/?q=<texto>`), `MainWindow` con un
  `QLineEdit` (Enter → `navigate`) sobre un `QWebEngineView`, y `main.cpp` que crea un
  `QWebEngineProfile("default")` y abre `argv[1]` o `about:blank`. Tests con
  `QT_QPA_PLATFORM=offscreen` y `QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox` solo en CI/contenedor.
- [ ] **Step 4:** `ctest --test-dir build --output-on-failure` → PASS.
- [ ] **Step 5:** Commit `feat: esqueleto CMake y ventana mínima que navega`.

### Tarea 2: Crate `adblock-ffi`

**Files:**
- Create: `adblock-ffi/Cargo.toml`, `adblock-ffi/src/lib.rs`, `adblock-ffi/include/sora_adblock.h`

**Interfaces (C, en `sora_adblock.h`):**
```c
typedef struct SoraAdblock SoraAdblock;
SoraAdblock* sora_adblock_from_rules(const char* rules, size_t len);      /* nunca NULL */
SoraAdblock* sora_adblock_deserialize(const uint8_t* data, size_t len);   /* NULL si inválido */
size_t       sora_adblock_serialize(const SoraAdblock*, uint8_t** out);    /* liberar con sora_adblock_free_buffer */
void         sora_adblock_free_buffer(uint8_t* buf, size_t len);
bool         sora_adblock_should_block(const SoraAdblock*, const char* url,
                                       const char* source_url, const char* request_type);
void         sora_adblock_free(SoraAdblock*);
```
`request_type` es uno de: `document subdocument stylesheet script image font object xmlhttprequest
ping media websocket other`. Bloquea si `matched && !exception` (o `important`).

- [ ] **Step 1: Tests Rust que fallan** (`#[cfg(test)]` en `lib.rs`), con reglas `||ads.example^` y
  `@@||ads.example/ok^`:
  - `blocks_matching`: `https://ads.example/x.js`, origen `https://site.test`, `script` → `true`.
  - `exception_wins`: `https://ads.example/ok/x.js` → `false`.
  - `non_http_never_blocks`: `data:text/plain,hi`, `blob:https://site.test/1`, `about:blank` → `false`, sin pánico.
  - `serialize_roundtrip`: serializar y deserializar conserva `blocks_matching`.
  - `garbage_deserialize_is_null`: `sora_adblock_deserialize(b"basura")` → NULL.
- [ ] **Step 2:** `cargo test --manifest-path adblock-ffi/Cargo.toml` → falla.
- [ ] **Step 3:** Implementar con `adblock = "0.13"`, `crate-type = ["staticlib", "rlib"]`; el motor
  dentro de `SoraAdblock` debe ser `Send + Sync` (si `Engine` no lo es en esta versión, envolver en
  `Mutex` y anotarlo en un comentario). Cualquier `Err` o pánico interno → `false`/NULL
  (`catch_unwind` en cada función exportada).
- [ ] **Step 4:** `cargo test` → PASS.
- [ ] **Step 5:** Commit `feat: crate adblock-ffi sobre el motor de Brave`.

### Tarea 3: Integración con CMake y envoltorio `AdblockEngine`

**Files:**
- Modify: `CMakeLists.txt` (Corrosion vía `FetchContent`, tag `v0.5.2`; `corrosion_import_crate`)
- Create: `src/privacy/AdblockEngine.h/.cpp`, `tests/tst_adblockengine.cpp`, `tests/data/rules-small.txt`

**Interfaces:**
- Consumes: API C de la Tarea 2.
- Produces:
  ```cpp
  namespace sora {
  class AdblockEngine {            // no copiable, movible
  public:
      static AdblockEngine fromRules(const QByteArray& rules);
      static std::optional<AdblockEngine> deserialize(const QByteArray& data);
      QByteArray serialize() const;
      bool shouldBlock(const QUrl& url, const QUrl& firstParty, const char* requestType) const; // thread-safe
  };
  }
  ```

- [ ] **Step 1: Tests que fallan** en `tst_adblockengine.cpp` con `rules-small.txt` (`||ads.example^`,
  `@@||ads.example/ok^`, `||tracker.test^$third-party`):
  - `ads.example/x.js` bloqueado; `ads.example/ok/x.js` no.
  - `tracker.test/p.gif` desde `https://site.test` (tercero) bloqueado; desde `https://tracker.test` no.
  - `concurrentChecks`: 8 hilos × 10 000 llamadas a `shouldBlock` → resultados correctos, sin fallos.
  - `perCheckCost`: media de 10 000 llamadas con EasyList-like de 2 000 reglas generadas < 50 µs.
- [ ] **Step 2:** compilar y ejecutar → falla.
- [ ] **Step 3:** Implementar el envoltorio (un `std::unique_ptr<SoraAdblock, Deleter>`).
- [ ] **Step 4:** `ctest` → PASS.
- [ ] **Step 5:** Commit `feat: AdblockEngine en C++ enlazado con Corrosion`.

### Tarea 4: `RequestInterceptor` conectado al perfil

**Files:**
- Create: `src/privacy/RequestInterceptor.h/.cpp`, `tests/tst_requestinterceptor.cpp`
- Modify: `src/app/main.cpp`, `src/app/MainWindow.cpp` (mostrar "Bloqueados: N" en la barra de estado)

**Interfaces:**
- Consumes: `sora::AdblockEngine`.
- Produces:
  ```cpp
  const char* sora::adblockRequestType(QWebEngineUrlRequestInfo::ResourceType);
  class sora::RequestInterceptor : public QWebEngineUrlRequestInterceptor {
      void setEngine(std::shared_ptr<const AdblockEngine>);   // atómico; nullptr = no bloquear
      bool decide(const QUrl& url, const QUrl& firstParty,
                  QWebEngineUrlRequestInfo::ResourceType) const;  // lógica pura, testeable
      void interceptRequest(QWebEngineUrlRequestInfo&) override;  // llama a decide()
  signals:
      void blocked(QUrl url);                                    // emitida con conexión en cola
  };
  ```

- [ ] **Step 1: Tests que fallan:**
  - `adblockRequestType(ResourceTypeScript) == "script"`, `SubFrame → "subdocument"`,
    `MainFrame → "document"`, `Xhr → "xmlhttprequest"`, `Image → "image"`, desconocido → `"other"`.
  - `decide` sin motor → `false` para cualquier URL (motor aún cargando).
  - `decide` con motor de `rules-small.txt` → bloquea `ads.example/x.js`.
  - `decide` nunca bloquea `ResourceTypeMainFrame` (navegar a un dominio de la lista debe funcionar).
- [ ] **Step 2:** ejecutar → falla.
- [ ] **Step 3:** Implementar; `setEngine` usa `std::atomic<std::shared_ptr<const AdblockEngine>>`
  (o `std::atomic_load/store` si el compilador no lo soporta). Registrar con
  `profile->setUrlRequestInterceptor`.
- [ ] **Step 4:** `ctest` → PASS.
- [ ] **Step 5:** Commit `feat: interceptor de peticiones con el bloqueador`.

### Tarea 5: `FilterListStore` — listas y caché del motor sin bloquear el arranque

**Files:**
- Create: `src/privacy/FilterListStore.h/.cpp`, `tests/tst_filterliststore.cpp`
- Modify: `src/app/main.cpp`

**Interfaces:**
- Consumes: `AdblockEngine`, `RequestInterceptor::setEngine`.
- Produces:
  ```cpp
  class sora::FilterListStore : public QObject {
      FilterListStore(QString dir, QList<QUrl> lists, QNetworkAccessManager*, QObject* parent = nullptr);
      void start();   // asíncrono; nunca bloquea el hilo de interfaz
  signals:
      void engineReady(std::shared_ptr<const sora::AdblockEngine>);
      void failed(QString reason);   // en español
  };
  ```
  Disco: `<dir>/lists/<nombre>.txt` y `<dir>/engine.bin` + `<dir>/engine.version` (versión del crate
  y hash de las listas). `dir` en la app = `QStandardPaths::AppLocalDataLocation + "/adblock"`.

- [ ] **Step 1: Tests que fallan** (directorio temporal, `QNetworkAccessManager` apuntando a un
  servidor local `QTcpServer` mínimo o listas ya copiadas):
  - `usesCachedEngine`: con `engine.bin` válido → `engineReady` sin tocar la red.
  - `rebuildsOnCorruptCache`: `engine.bin` con basura → recompila desde `lists/`, emite `engineReady`
    y reescribe un `engine.bin` válido.
  - `noNetworkFirstRun`: sin listas y red caída → emite `failed`, no `engineReady`, sin cuelgue.
  - `downloadsMissingLists`: servidor local sirve `rules-small.txt` → listas guardadas y `engineReady`.
- [ ] **Step 2:** ejecutar → falla.
- [ ] **Step 3:** Implementar: compilación/deserialización en `QtConcurrent::run`; descargas con el
  `QNetworkAccessManager` dado; tras compilar, escribir `engine.bin` de forma atómica (`QSaveFile`).
- [ ] **Step 4:** `ctest` → PASS.
- [ ] **Step 5:** Commit `feat: listas de filtros y caché del motor en segundo plano`.

### Tarea 6: Medición de arranque y memoria

**Files:**
- Modify: `src/app/main.cpp` (opción `--bench`)
- Create: `bench/bench.ps1`, `bench/README.md`

**Interfaces:**
- Produces: con `--bench`, Sora escribe en stdout una línea JSON al cargar la primera página:
  `{"window_shown_ms":N,"first_load_ms":N}` medidos desde el inicio de `main()` y sale.
- `bench.ps1 -Sora <ruta sora.exe> -Chrome <ruta chrome.exe> -Runs 10 -Url <url>`:
  para cada navegador, con perfil temporal limpio: tiempo hasta que el proceso tiene
  `MainWindowHandle` ≠ 0 (arranque en frío), y suma de `PrivateMemorySize64` del árbol de procesos
  15 s después de abrir `-Url`. Imprime tabla con mediana y p90 de cada métrica.

- [ ] **Step 1:** Test manual en Linux: `./build/src/sora --bench https://example.com` imprime el JSON
  con dos enteros > 0 y sale con código 0.
- [ ] **Step 2:** Implementar `--bench` y `bench.ps1` (PowerShell 5.1 compatible, sin módulos externos).
- [ ] **Step 3:** Verificar sintaxis: `pwsh -NoProfile -Command "[ScriptBlock]::Create((Get-Content bench/bench.ps1 -Raw)) | Out-Null"`
  (si `pwsh` no está en el contenedor, se valida en el job de Windows de la Tarea 7).
- [ ] **Step 4:** Commit `feat: modo --bench y script de comparación con Chrome`.

### Tarea 7: CI en Linux y Windows con artefacto descargable

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1:** Workflow con matriz `ubuntu-24.04` y `windows-2022`: instalar Qt 6.12.0 + módulos
  `qtwebengine qtwebchannel qtpositioning` con `jurplel/install-qt-action@v4`; Rust estable; en
  Windows usar el entorno MSVC (`ilammy/msvc-dev-cmd@v1`); `cmake` + `ninja`; `ctest` con
  `QT_QPA_PLATFORM=offscreen`; `cargo test`.
- [ ] **Step 2:** En Windows: `windeployqt --release sora.exe` en una carpeta `Sora/`, comprimir y subir
  como artefacto `sora-windows-x64`. Ejecutar además el parse-check de `bench.ps1`.
- [ ] **Step 3:** Empujar la rama y comprobar que ambos jobs terminan en verde.
- [ ] **Step 4:** Commit `ci: compilación y pruebas en Linux y Windows`.

### Tarea 8: Medición en el PC del usuario (con su permiso)

- [ ] **Step 1:** Conectar al PC del usuario (Remote Control) cuando lo autorice.
- [ ] **Step 2:** Descargar el artefacto `sora-windows-x64` del último CI verde y ejecutar
  `bench.ps1` contra el Chrome instalado (`-Runs 10`, `-Url https://es.wikipedia.org/wiki/Navegador_web`).
- [ ] **Step 3:** Guardar resultados en `bench/results/2026-xx-xx-hito1.md` con versión de Chrome,
  CPU, RAM y versión de Windows, y commit.
- [ ] **Step 4:** Presentar resultados al usuario. Si Sora no iguala a Chrome en arranque o memoria con
  1 pestaña, parar y decidir con el usuario (spec §7).

---

## Hitos siguientes (cada uno con su propio plan)
2. Interfaz Niebla: ventana sin marco, Snap Layouts, pestañas en la barra de título, barra de
   herramientas, barra de direcciones con sugerencias, tema claro/oscuro.
3. Datos: historial, marcadores y barra de favoritos, restaurar sesión, pestañas fijadas, Ctrl+Shift+T.
4. Privacidad: escudo con detalle y excepciones por sitio, filtros cosméticos, actualización diaria de
   listas, HTTPS solo, cookies de terceros, WebRTC, DNS sobre HTTPS, Safe Browsing.
5. Rendimiento: pestañas dormidas.
6. Descargas, modo privado, ajustes.
7. Instalador Inno Setup con autoactualización firmada.
- Experimento DRM (Netflix/Crunchyroll) en el PC del usuario, en paralelo a partir del hito 2.
