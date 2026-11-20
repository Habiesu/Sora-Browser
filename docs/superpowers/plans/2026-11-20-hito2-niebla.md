# Hito 2 — Interfaz Niebla: plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sustituir la ventana mínima del Hito 1 por la ventana Niebla: pestañas en la barra de
título, botones de ventana a la derecha, barra de herramientas (atrás, adelante, recargar, dirección
redondeada con candado, escudo con bloqueos de la pestaña, estrella, menú), barra de favoritos, tema
claro y oscuro, sin perder la ventaja de arranque y memoria frente a Chrome.

**Architecture:** Qt Widgets. `TabManager` (browser/) gestiona las pestañas (`Tab`, un
`QWebEngineView` con su propio interceptor para contar bloqueos por pestaña) dentro de un
`QStackedWidget`; la interfaz (ui/) solo pinta y reenvía señales. El bloqueo pasa de un interceptor
de perfil a un `AdblockService` compartido que consultan los interceptores de cada página. En Windows
la ventana usa `Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint | Qt::CustomizeWindowHint`
(probado en este PC: conserva el marco nativo de Windows 11 —sombra, esquinas redondeadas, bordes— y
deja la barra de título libre) y `WM_NCHITTEST` para arrastrar, redimensionar y Snap Layouts
(`HTMAXBUTTON`). En Linux, `Qt::FramelessWindowHint` con `startSystemMove/startSystemResize`.

**Tech Stack:** Qt 6.12.0 (Widgets, WebEngineWidgets, Test), C++20, Win32 (`WM_NCHITTEST`,
`WM_NCMOUSE*`). Sin dependencias nuevas: los iconos se dibujan con `QPainterPath` (no Qt Svg, que
añadiría DLL y plugin al arranque).

**Spec:** `docs/superpowers/specs/2026-11-18-sora-browser-design.md` (§5 "Estética Niebla", §6
"Módulos" y "Ventana Niebla", §7 "Rendimiento", §9). Boceto: sección E de
https://claude.ai/artifact/JcdkoVfAPYLPF1npdeXcAa.

## Global Constraints
- C++20, Qt Widgets (sin QML ni HTML en la interfaz). Colores planos, sin transparencias.
- Textos visibles en español.
- El arranque no puede empeorar: Sora debe seguir ≤ Chrome en arranque y memoria con una pestaña
  (`bench.ps1`, Hito 1: Sora 209 ms / 249,8 MB; Chrome 247 ms / 259,2 MB). Nada pesado antes de
  `window.show()`.
- Pruebas primero (Qt Test en `tests/`), con `QT_QPA_PLATFORM=offscreen` (Qt WebEngine funciona así
  en Windows; comprobado).
- Atajos como Chrome: Ctrl+T, Ctrl+W, Ctrl+Tab, Ctrl+Shift+Tab, Ctrl+1…8, Ctrl+9 (última), Ctrl+L /
  Alt+D / F6, F5 / Ctrl+R, Alt+Izquierda / Alt+Derecha, Esc (parar).
- Fuera de alcance (otros hitos): marcadores reales y estrella funcional (Hito 3), panel del escudo,
  HTTPS y cookies (Hito 4), pestañas dormidas (Hito 5), descargas y ajustes (Hito 6).

### Paleta Niebla (§9: "paleta exacta a criterio del constructor, partiendo del boceto")

| Papel | Claro (boceto) | Oscuro |
|---|---|---|
| `window` (barra de título, pestaña inactiva) | `#e9edf3` | `#1b1f27` |
| `surface` (pestaña activa, barra de herramientas y de favoritos) | `#f8fafc` | `#252a33` |
| `text` | `#1c2330` | `#e6e9ef` |
| `mutedText` (pestaña inactiva, favoritos) | `#5a6476` | `#a3abb9` |
| `faintText` (× de pestaña, ruta en la dirección, iconos desactivados) | `#9aa3b2` | `#6b7383` |
| `line` (separadores) | `#dfe4ec` | `#323946` |
| `field` (fondo de la dirección) | `#eaeef4` | `#1b1f27` |
| `hover` (botones y pestañas al pasar) | `#dde3eb` | `#2f3540` |
| `shield` (escudo y candado) | `#2b8a3e` | `#51cf66` |
| `closeHover` (cerrar ventana al pasar; icono blanco) | `#c42b1c` | `#c42b1c` |

Medidas (a 100 % de escala): barra de título 40 px, pestaña de 60 a 220 px de ancho con radio 8 px
arriba, botones de ventana 46 × 40 px, barra de herramientas 40 px con botones de 32 × 32 px y radio
6 px, dirección de 32 px de alto con radio 8 px, barra de favoritos 30 px, iconos de 16 px. Fuente
del sistema (Segoe UI Variable en Windows 11), 9 pt.

## Review Focus
1. **Cerrar la última pestaña** (Ctrl+W o ×) → se cierra la ventana sin fallar (test en Tarea 4).
2. **Webs que abren ventanas** (`target=_blank`, `window.open`) → se abren en pestaña nueva junto a
   la actual, no se pierden (test en Tarea 4).
3. **Muchas pestañas** (30) → se encogen hasta el mínimo y el botón "+" sigue visible dentro de la
   barra (test en Tarea 5).
4. **Ventana maximizada** → sin bordes de redimensión y la barra de título sigue arrastrable (test
   en Tarea 6).
5. **Escribir en la dirección mientras la página cambia de URL** (redirecciones, cargas lentas) → no
   se pisa lo que el usuario está escribiendo (test en Tarea 7).

---

## Estructura de archivos

```
src/privacy/AdblockService.{h,cpp}   motor compartido y seguro entre hilos (nuevo)
src/privacy/RequestInterceptor.{h,cpp}  pasa a ser por página y consulta AdblockService
src/browser/Tab.{h,cpp}              QWebEngineView + interceptor + contador de bloqueos
src/browser/TabManager.{h,cpp}       crea, cierra, mueve y selecciona pestañas en un QStackedWidget
src/ui/Theme.{h,cpp}                 paleta Niebla clara/oscura y hoja de estilos
src/ui/Icons.{h,cpp}                 iconos dibujados con QPainterPath
src/ui/TabStrip.{h,cpp}              QTabBar con "+" y cierre con botón central
src/ui/WindowFrame.{h,cpp}           WindowHit y windowHitTest (lógica pura)
src/ui/TitleBar.{h,cpp}              TabStrip + botones de ventana; hitAt()
src/ui/Toolbar.{h,cpp}               atrás/adelante/recargar, Omnibox, escudo, estrella, menú
src/ui/Omnibox.{h,cpp}               dirección: displayUrl(), candado, edición sin pisar
src/ui/BookmarksBar.{h,cpp}          barra vacía hasta el Hito 3
src/app/MainWindow.{h,cpp}           monta todo, atajos, nativeEvent (Windows), tema
src/app/main.cpp                     AdblockService en vez de interceptor de perfil
tests/tst_requestinterceptor.cpp, tst_theme.cpp, tst_icons.cpp, tst_tabmanager.cpp,
tests/tst_tabstrip.cpp, tst_windowframe.cpp, tst_mainwindow.cpp
```

`src/CMakeLists.txt` añade cada archivo nuevo a `sora_core`; `tests/CMakeLists.txt` añade cada
prueba nueva con `sora_add_test(...)`. En Windows `sora_core` enlaza también `dwmapi` y `user32`.

---

### Task 1: AdblockService y un interceptor por página

**Files:**
- Create: `src/privacy/AdblockService.h`, `src/privacy/AdblockService.cpp`
- Modify: `src/privacy/RequestInterceptor.{h,cpp}`, `src/app/main.cpp`, `src/CMakeLists.txt`
- Test: `tests/tst_requestinterceptor.cpp`

**Interfaces:**
- Produces:
  - `class sora::AdblockService : public QObject` con
    `void setEngine(std::shared_ptr<const AdblockEngine> engine)` (atómico; nullptr = no bloquear) y
    `bool decide(const QUrl& url, const QUrl& firstParty, QWebEngineUrlRequestInfo::ResourceType type) const`
    (la lógica actual de `RequestInterceptor::decide`, que se mueve aquí sin cambios).
  - `sora::RequestInterceptor(const AdblockService* service, QObject* parent = nullptr)`;
    `interceptRequest` bloquea si `service->decide(...)` y emite `blocked(QUrl)`. Se quitan
    `setEngine` y `decide` del interceptor.

- [ ] **Step 1:** Reescribir los tests: `noEngineMeansNoBlocking`, `blocksWithEngine`,
  `neverBlocksMainFrameNavigation`, `engineCanBeRemoved` pasan a usar `sora::AdblockService`
  (mismas aserciones). Nuevo `interceptorsShareTheService`: dos `RequestInterceptor` sobre el mismo
  servicio; tras `service.setEngine(smallEngine())` ambos ven el motor (comprobar con
  `service.decide(...)` y que construir dos interceptores no copia estado).
- [ ] **Step 2:** `cmake --build build-win && ctest --test-dir build-win -R requestinterceptor` →
  falla al compilar (no existe `AdblockService`).
- [ ] **Step 3:** Implementar `AdblockService` y adaptar `RequestInterceptor`. En `main.cpp`, crear
  el servicio, conectar `FilterListStore::engineReady` a `AdblockService::setEngine` y dejar de
  llamar a `profile->setUrlRequestInterceptor` (los interceptores se ponen por página en la Tarea 4;
  hasta entonces `MainWindow` pone uno en su página para no perder el bloqueo).
- [ ] **Step 4:** Compilar y pasar todas las pruebas → PASS.
- [ ] **Step 5:** Commit `refactor: AdblockService compartido e interceptor por página`.

### Task 2: Tema Niebla claro y oscuro

**Files:**
- Create: `src/ui/Theme.h`, `src/ui/Theme.cpp`
- Test: `tests/tst_theme.cpp`

**Interfaces:**
- Produces:
  - `struct sora::Palette { QColor window, surface, text, mutedText, faintText, line, field, hover, shield, closeHover; };`
  - `sora::Palette sora::nieblaPalette(Qt::ColorScheme scheme)` (Dark → oscuro; Light y Unknown → claro), valores de la tabla de Global Constraints.
  - `QString sora::nieblaStyleSheet(const sora::Palette& p)` — hoja de estilos de toda la ventana. Selecciona por `objectName`: `TitleBar`, `Toolbar`, `BookmarksBar`, `Omnibox`, `ShieldButton`, `WindowButton`, `CloseButton`, y por clase `QTabBar::tab` (`:selected`, `:hover`). Medidas de Global Constraints.

- [ ] **Step 1:** Tests: `lightMatchesSketch` (`window == #e9edf3`, `surface == #f8fafc`,
  `text == #1c2330`, `shield == #2b8a3e`, `closeHover == #c42b1c`); `unknownIsLight`;
  `darkIsDark` (luminancia relativa de `window` < 0,05); `textIsReadable` (contraste WCAG de `text`
  sobre `surface` y sobre `window` ≥ 7, y de `mutedText` sobre `window` ≥ 4,5, en los dos temas);
  `styleSheetUsesPalette` (`nieblaStyleSheet(light)` contiene `#e9edf3` y `#c42b1c`, y la oscura
  contiene `#1b1f27`).
- [ ] **Step 2:** Compilar → falla (no existe `Theme.h`).
- [ ] **Step 3:** Implementar. Contraste con la fórmula WCAG 2.x (luminancia relativa sRGB) como
  función auxiliar en el test.
- [ ] **Step 4:** Pruebas → PASS.
- [ ] **Step 5:** Commit `feat(ui): paleta Niebla clara y oscura`.

### Task 3: Iconos dibujados

**Files:**
- Create: `src/ui/Icons.h`, `src/ui/Icons.cpp`
- Test: `tests/tst_icons.cpp`

**Interfaces:**
- Produces: `enum class sora::Icon { Back, Forward, Reload, Stop, Lock, Globe, Shield, Star, Menu, Plus, Close, Minimize, Maximize, Restore };`
  y `QIcon sora::icon(sora::Icon which, const QColor& color)`.

- [ ] **Step 1:** Test `everyIconDraws`: para cada valor del enum, `icon(i, Qt::black).pixmap(16, 16)`
  no es nulo, tiene algún píxel con alfa > 0 y no está lleno entero. `usesRequestedColor`: el píxel
  más opaco de `icon(Icon::Shield, QColor("#2b8a3e"))` es ese color.
- [ ] **Step 2:** Compilar → falla.
- [ ] **Step 3:** Implementar con `QIconEngine` propio (pinta a cualquier tamaño y escala de
  pantalla). Trazados en una caja de 24 × 24 copiados del boceto: atrás `M15 18l-6-6 6-6`, adelante
  `M9 18l6-6-6-6`, recargar `M21 12a9 9 0 1 1-3-6.7L21 8M21 3v5h-5`, candado (rect 5,11,14×10 r2 +
  arco `M8 11V7a4 4 0 0 1 8 0v4`), escudo relleno `M12 2l8 3v6c0 5-3.4 9.4-8 11-4.6-1.6-8-6-8-11V5z`,
  estrella `M12 3l2.7 5.6 6.1.9-4.4 4.3 1 6.1L12 17l-5.4 2.9 1-6.1L3.2 9.5l6.1-.9z`, menú tres
  círculos r 1,8 en x = 5, 12, 19. Los de ventana en caja de 12 × 12 con trazo 1: minimizar
  `M1.5 6h9`, maximizar rect 1,5 a 10,5, restaurar (dos rects desplazados 2 px), cerrar
  `M1.5 1.5l9 9M10.5 1.5l-9 9`. Parar = cerrar a escala 24; "+" y globo a criterio. Trazo 2 px,
  extremos redondeados.
- [ ] **Step 4:** Pruebas → PASS.
- [ ] **Step 5:** Commit `feat(ui): iconos Niebla dibujados con QPainterPath`.

### Task 4: Pestañas (Tab y TabManager)

**Files:**
- Create: `src/browser/Tab.{h,cpp}`, `src/browser/TabManager.{h,cpp}`
- Test: `tests/tst_tabmanager.cpp`

**Interfaces:**
- Consumes: `AdblockService`, `RequestInterceptor(const AdblockService*, QObject*)` (Tarea 1).
- Produces:
  - `class sora::Tab : public QWebEngineView` — `Tab(QWebEngineProfile*, const AdblockService*, QWidget* parent = nullptr)`;
    `int blockedCount() const`; `void countBlocked()`; señal `blockedCountChanged(int)`; el contador
    vuelve a 0 en `loadStarted`. Cada `Tab` crea su `QWebEnginePage` y su `RequestInterceptor`
    (`page->setUrlRequestInterceptor`), con `blocked` conectado en cola a `countBlocked`.
    `std::function<Tab*(QWebEnginePage::WebWindowType)> onCreateWindow;` y
    `QWebEngineView* createWindow(QWebEnginePage::WebWindowType) override` público que lo llama.
  - `class sora::TabManager : public QObject` — `TabManager(QWebEngineProfile*, const AdblockService*, QStackedWidget* stack, QObject* parent = nullptr)`;
    `Tab* newTab(const QUrl& url = {}, int index = -1, bool activate = true)` (−1 = al final);
    `void closeTab(int index)`; `void moveTab(int from, int to)`; `void setCurrentIndex(int)`;
    `int currentIndex() const`; `int count() const`; `Tab* tab(int) const`; `Tab* currentTab() const`;
    `int indexOf(const Tab*) const`.
    Señales: `tabInserted(int index)`, `tabRemoved(int index)`, `currentChanged(int index)`,
    `tabTitleChanged(int index, const QString&)`, `tabIconChanged(int index, const QIcon&)`,
    `lastTabClosed()`.
    `createWindow`: `NewBackgroundTab` → pestaña junto a la actual sin activarla; `NewWebTab` y
    `NewWindow`/`NewDialog` → junto a la actual y activada.
    Cerrar la pestaña actual en `i` selecciona la que queda en `i` (o `i-1` si era la última).

- [ ] **Step 1:** Tests (con `QStackedWidget` real; páginas con `setHtml`):
  `newTabAddsAndActivates` (count 1→2, current = 1, `tabInserted(1)`);
  `closeCurrentSelectsNeighbor` (3 pestañas, actual 1, cerrar 1 → actual 1 y count 2; cerrar la
  última actual → actual = count-1); `closingLastTabEmitsLastTabClosed` (`QSignalSpy` cuenta 1);
  `titleChangeIsReported` (`setHtml("<title>A</title>")` → `tabTitleChanged(0,"A")` con `QTRY_COMPARE`);
  `blockedCountIsPerTab` (`tab(0)->countBlocked()` dos veces → 2 en la 0 y 0 en la 1; recargar la 0
  → vuelve a 0); `popupOpensNextToCurrent` (3 pestañas, actual 0, `tab(0)->createWindow(NewWebTab)`
  → devuelve `tab(1)`, count 4, actual 1; con `NewBackgroundTab` la actual no cambia);
  `moveKeepsStackInSync` (`moveTab(0,2)` → `tab(2)` es la que era 0).
- [ ] **Step 2:** Compilar → falla.
- [ ] **Step 3:** Implementar. `QStackedWidget` es la fuente de verdad del orden; `moveTab` quita y
  reinserta el widget. Al cambiar de pestaña, el foco va a la vista.
- [ ] **Step 4:** Pruebas → PASS.
- [ ] **Step 5:** Commit `feat(browser): pestañas con contador de bloqueos propio`.

### Task 5: Tira de pestañas

**Files:**
- Create: `src/ui/TabStrip.{h,cpp}`
- Test: `tests/tst_tabstrip.cpp`

**Interfaces:**
- Consumes: `sora::icon(Icon::Plus/Close, ...)` (Tarea 3).
- Produces: `class sora::TabStrip : public QTabBar` — pestañas cerrables, movibles, `elideMode = Qt::ElideRight`,
  `expanding = false`, `drawBase = false`, `documentMode = true`, botones de desplazamiento si no caben.
  `tabSizeHint` entre 60 y 220 px de ancho: reparte el ancho disponible (sin el "+") entre las
  pestañas. Botón "+" (`QToolButton`, `objectName` "NewTabButton") colocado justo tras la última
  pestaña. Señal `newTabRequested()`. Botón central del ratón sobre una pestaña → `tabCloseRequested(i)`.
  `void setPalette(const Palette&)` para el color de la × (`faintText`).

- [ ] **Step 1:** Tests: `plusRequestsNewTab` (clic en "NewTabButton" → `newTabRequested` 1 vez);
  `middleClickCloses` (`QTest::mouseClick(Qt::MiddleButton)` en el centro de `tabRect(1)` →
  `tabCloseRequested(1)`); `widthIsClamped` (barra de 1200 px con 2 pestañas → 220 px cada una; con
  30 pestañas → 60 px); `plusStaysVisible` (30 pestañas en 1200 px → el "+" queda dentro de
  `rect()`).
- [ ] **Step 2:** Compilar → falla.
- [ ] **Step 3:** Implementar.
- [ ] **Step 4:** Pruebas → PASS.
- [ ] **Step 5:** Commit `feat(ui): tira de pestañas con botón de nueva pestaña`.

### Task 6: Barra de título y marco de ventana

**Files:**
- Create: `src/ui/WindowFrame.{h,cpp}`, `src/ui/TitleBar.{h,cpp}`
- Modify: `src/CMakeLists.txt` (en `WIN32`, enlazar `dwmapi user32`)
- Test: `tests/tst_windowframe.cpp`

**Interfaces:**
- Consumes: `TabStrip` (Tarea 5), `icon(...)` (Tarea 3).
- Produces:
  - `enum class sora::WindowHit { Client, Caption, Minimize, Maximize, Close, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };`
  - `sora::WindowHit sora::windowHitTest(QPoint pos, QSize size, int border, bool maximized, sora::WindowHit titleBarHit)`:
    sin maximizar, los `border` px de cada lado (y las esquinas, con `2*border` de alcance) ganan a
    todo; si no, devuelve `titleBarHit`.
  - `class sora::TitleBar : public QWidget` (`objectName` "TitleBar", alto 40 px) con `TabStrip* tabs() const`,
    botones `minimizeButton()`, `maximizeButton()`, `closeButton()` (`QToolButton` de 46 × 40,
    `objectName` "WindowButton" / "CloseButton"), `WindowHit hitAt(QPoint local) const`
    (botones → su valor; pestaña o "+" → Client; el resto → Caption),
    `void setMaximized(bool)` (icono Restore/Maximize) y `void setMaximizeHovered(bool)`
    (propiedad dinámica "hovered" para la hoja de estilos, porque en Windows el ratón sobre
    maximizar llega como evento no cliente).
    Señales `minimizeRequested()`, `maximizeToggleRequested()`, `closeRequested()`.
    Fuera de Windows, clic en Caption → `window()->windowHandle()->startSystemMove()` y doble clic →
    `maximizeToggleRequested()`.

- [ ] **Step 1:** Tests: `edgesWinWhenNormal` (tamaño 800×600, borde 6: (0,300) → Left,
  (799,300) → Right, (400,0) → Top, (2,2) → TopLeft, (797,597) → BottomRight, (400,20) → el
  `titleBarHit` dado); `noEdgesWhenMaximized` ((0,300) y (400,0) con `maximized` → el `titleBarHit`);
  `titleBarRegions` (TitleBar de 1000×40 con 2 pestañas: centro de `closeButton()` → Close, de
  `maximizeButton()` → Maximize, centro de `tabRect(0)` → Client, un punto vacío entre el "+" y los
  botones → Caption); `buttonsSignal` (clic en cada botón emite su señal).
- [ ] **Step 2:** Compilar → falla.
- [ ] **Step 3:** Implementar.
- [ ] **Step 4:** Pruebas → PASS.
- [ ] **Step 5:** Commit `feat(ui): barra de título Niebla y prueba de zonas de la ventana`.

### Task 7: Barra de herramientas, dirección, escudo y favoritos

**Files:**
- Create: `src/ui/Toolbar.{h,cpp}`, `src/ui/Omnibox.{h,cpp}`, `src/ui/BookmarksBar.{h,cpp}`
- Test: `tests/tst_mainwindow.cpp` (añadir casos)

**Interfaces:**
- Consumes: `icon(...)`, `Palette` (Tareas 2–3).
- Produces:
  - `QString sora::displayUrl(const QUrl&)`: `https://` y `www.` fuera, el resto tal cual
    (`http://` se muestra); `about:blank` → cadena vacía.
  - `class sora::Omnibox : public QLineEdit` (`objectName` "Omnibox", placeholder "Busca o escribe
    una dirección") con `void setUrl(const QUrl&)` (muestra `displayUrl`; si el usuario está
    editando —tiene foco y `isModified()`— solo guarda la URL y no toca el texto), candado (Lock,
    color `shield`) para https y globo (`faintText`) para lo demás como acción a la izquierda. Al
    recibir el foco con el ratón selecciona todo; Esc restaura la URL y quita el foco. Señal
    `submitted(const QString&)`.
  - `class sora::Toolbar : public QWidget` (`objectName` "Toolbar", 40 px) con `Omnibox* omnibox()`,
    `void setNavigation(bool canBack, bool canForward, bool loading)` (recargar ↔ parar),
    `void setBlockedCount(int)` (botón `objectName` "ShieldButton": icono Shield + número, tooltip
    "Bloqueados en esta página: N"), estrella desactivada (tooltip "Marcadores: llegan en el próximo
    hito"), menú (`QMenu`: "Nueva pestaña", "Cerrar pestaña", separador, "Salir").
    Señales `backRequested()`, `forwardRequested()`, `reloadRequested()`, `stopRequested()`,
    `newTabRequested()`, `closeTabRequested()`, `quitRequested()`.
  - `class sora::BookmarksBar : public QWidget` (`objectName` "BookmarksBar", 30 px) con el texto
    atenuado "Aún no hay marcadores".

- [ ] **Step 1:** Tests: `displayUrlHidesHttpsAndWww` (`https://www.example.com/a?b` →
  `example.com/a?b`; `http://x.test/` → `http://x.test/`; `about:blank` → `""`);
  `omniboxKeepsWhatUserTypes` (foco + `QTest::keyClicks("hola")` y luego `setUrl(https://x.test)` →
  el texto sigue "hola"; sin foco, `setUrl` cambia el texto a `x.test`); `escapeRestoresUrl`;
  `submitEmitsText` (Enter → `submitted("hola")`); `shieldShowsCount` (`setBlockedCount(14)` → el
  texto del botón contiene "14"); `reloadBecomesStop` (`setNavigation(false,false,true)` → clic emite
  `stopRequested`).
- [ ] **Step 2:** Compilar → falla.
- [ ] **Step 3:** Implementar.
- [ ] **Step 4:** Pruebas → PASS.
- [ ] **Step 5:** Commit `feat(ui): barra de herramientas, dirección y escudo Niebla`.

### Task 8: MainWindow Niebla, atajos y Windows

**Files:**
- Modify: `src/app/MainWindow.{h,cpp}`, `src/app/main.cpp`
- Test: `tests/tst_mainwindow.cpp` (añadir casos)

**Interfaces:**
- Consumes: todo lo anterior.
- Produces: `MainWindow(QWebEngineProfile*, const AdblockService*, QWidget* parent = nullptr)`;
  `TabManager* tabs() const`; `Tab* currentTab() const` (sustituye a `view()`; `main.cpp --bench`
  usa `currentTab()`); `void navigate(const QString& input)` (en la pestaña actual);
  `void applyColorScheme(Qt::ColorScheme)` (aplica `nieblaStyleSheet` y recolorea iconos; se llama
  al arrancar con `QGuiApplication::styleHints()->colorScheme()` y en `colorSchemeChanged`).
  Se quita la barra de estado; al pasar sobre un enlace, una etiqueta flotante abajo a la izquierda
  muestra la URL.

- [ ] **Step 1:** Tests: `ctrlTOpensTab` / `ctrlWClosesTab` (`QTest::keyClick(&w, Qt::Key_T, Qt::ControlModifier)`
  → 2 pestañas; Ctrl+W → 1); `ctrlNumberSelects` (3 pestañas, Ctrl+1 → 0, Ctrl+9 → 2);
  `tabTitleFollowsPage` (`setHtml("<title>Hola</title>")` → texto de la pestaña "Hola" y título de
  ventana "Hola — Sora"); `shieldFollowsCurrentTab` (`countBlocked` en la pestaña 0, cambiar a la 1
  → escudo "0", volver → "1"); `darkSchemeRestyles` (`applyColorScheme(Qt::ColorScheme::Dark)` →
  `styleSheet()` contiene `#1b1f27`); `closingLastTabClosesWindow` (Ctrl+W con 1 pestaña →
  `w.isVisible()` false).
- [ ] **Step 2:** Compilar → falla.
- [ ] **Step 3:** Implementar. Disposición vertical: TitleBar, Toolbar, BookmarksBar, QStackedWidget.
  Banderas de ventana: Windows `Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint | Qt::CustomizeWindowHint`;
  otros `Qt::Window | Qt::FramelessWindowHint`. `nativeEvent` (solo `Q_OS_WIN`):
  `WM_NCHITTEST` → `windowHitTest(pos local, size, borde, isMaximized(), titleBar->hitAt(...))`
  traducido a `HTCLIENT/HTCAPTION/HTMINBUTTON/HTMAXBUTTON/HTCLOSE/HTLEFT…`; el borde es
  `GetSystemMetricsForDpi(SM_CXSIZEFRAME)+GetSystemMetricsForDpi(SM_CXPADDEDBORDER)` en píxeles
  lógicos. Minimizar y cerrar devuelven `HTCLIENT` (son botones Qt normales); solo maximizar devuelve
  `HTMAXBUTTON` (Snap Layouts) y por eso hay que atender `WM_NCMOUSEMOVE`/`WM_NCMOUSELEAVE`
  (hover) y `WM_NCLBUTTONDOWN`/`WM_NCLBUTTONUP` con `HTMAXBUTTON` (alternar maximizado, devolver
  `true`). Fuera de Windows, `mousePressEvent` en los bordes → `startSystemResize`.
  `--bench` y la medición siguen igual.
- [ ] **Step 4:** Pruebas → PASS. Comprobación a mano en Windows (capturas en
  `docs/superpowers/plans/hito2-capturas/`): arrastrar, doble clic en la barra, redimensionar por los
  bordes, Snap Layouts al pasar sobre maximizar, cerrar en rojo al pasar, tema claro y oscuro.
- [ ] **Step 5:** Commit `feat: ventana Niebla con pestañas, atajos y marco nativo en Windows`.

### Task 9: Medir y cerrar el hito

**Files:**
- Create: `bench/results/2026-11-20-hito2.md`
- Modify: `CLAUDE.md` (Estado), `mejoras.md`

- [ ] **Step 1:** `scripts\build-windows.ps1` → pruebas e instalador OK.
- [ ] **Step 2:** Instalar en silencio (`dist\Sora-0.1.0-instalador.exe /VERYSILENT /SUPPRESSMSGBOXES`)
  y `bench\bench.ps1 -Sora "$env:LOCALAPPDATA\Programs\Sora\sora.exe" -Runs 10`. Criterio: arranque
  y memoria ≤ Chrome. Si no, parar y avisar al usuario (CLAUDE.md).
- [ ] **Step 3:** Guardar resultados con el formato de `2026-11-20-hito1.md`; Hito 2 hecho en
  CLAUDE.md; lo que quede, en `mejoras.md`.
- [ ] **Step 4:** Commit `bench: resultados del Hito 2` y push a `ccr-765bf7fa-sutn5o`.
