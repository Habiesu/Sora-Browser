# Sora — Diseño de la versión 1

Fecha: 2026-11-18 · Estado: borrador para aprobación

## 1. Qué es y por qué

Sora es un navegador web de escritorio, de momento para uso personal de su autor.
Sus prioridades, por este orden de importancia declarado: **rendimiento**, **privacidad**
y **seguridad**. Lo que debe distinguirlo de otros navegadores es el **rendimiento**.

No construimos un motor web propio (ver §4): Sora usa un motor probado y concentra el
esfuerzo en todo lo de alrededor, que es donde se gana velocidad percibida frente a Chrome:
bloquear antes de descargar, dormir pestañas, arrancar rápido y no cargar lo que no se usa.

## 2. Objetivos, no-objetivos y anti-objetivos

### Objetivos (v1)
- Navegación con **pestañas** en la barra de título (estilo Niebla).
- **Barra de direcciones** unificada: URL o búsqueda, con sugerencias de historial y marcadores.
- **Marcadores** con barra de favoritos visible.
- **Historial** consultable y borrable.
- **Descargas** con panel simple.
- **Modo privado** en ventana separada que no deja rastro en disco.
- **Bloqueador** de anuncios y rastreadores integrado (motor de Brave), con contador por página.
- **Pestañas dormidas**: las inactivas se congelan y luego se descartan para liberar memoria.
- **Restaurar sesión** al arrancar (activado por defecto, desactivable), **pestañas fijadas**
  y **reabrir pestaña cerrada** (Ctrl+Shift+T). *(Decisión del constructor: el usuario pidió
  todas las funciones habituales y el diseño de pestañas dormidas ya las supone.)*
- Tema claro y oscuro que sigue al sistema.
- Windows 10/11 x64 como plataforma principal; Linux x64 compilable y funcional.

### No-objetivos (v1)
- Sincronización entre dispositivos.
- Extensiones de terceros.
- Gestor de contraseñas propio (se puede añadir más adelante).
- Versión móvil y macOS.
- Instalador con autoactualización (v1 se distribuye como carpeta portable/zip).

### Anti-objetivos (fracaso aunque "funcione")
- Que Sora arranque o abra pestañas más lento que Chrome en el mismo equipo.
- Cualquier telemetría o conexión de fondo no pedida por el usuario
  (salvo la descarga de listas de bloqueo y lo indispensable de Chromium, documentado).
- Que la interfaz consuma memoria notable por sí misma (nada de interfaz en HTML/Electron).
- Mantener una bifurcación (fork) de Chromium o Firefox.

## 3. Restricciones
- Desarrollo en un contenedor Linux en la nube; la versión de Windows se compila y
  empaqueta en GitHub Actions. Las pruebas finales en Windows las hace el usuario.
- Licencia de Qt: LGPLv3, enlazado dinámico. Suficiente para uso personal.
- Qt WebEngine precompilado **no incluye códecs propietarios** (H.264/AAC) ni DRM
  Widevine garantizado: la mayoría de YouTube funciona (VP9/AV1), pero servicios como
  Netflix o Disney+ probablemente no. Compilar Qt WebEngine con códecs propios es posible
  pero cuesta horas de compilación por versión; queda fuera de v1.
- Qt WebEngine no ofrece Google Safe Browsing. La protección contra webs maliciosas
  vendrá de las listas de bloqueo (p. ej. listas de malware de uBlock).

## 4. Enfoques considerados

| Enfoque | A favor | En contra | Decisión |
|---|---|---|---|
| **Qt 6 + Qt WebEngine (C++)** | Motor Chromium rápido y con sandbox; perfiles, interceptor de peticiones, descargas y ciclo de vida de pestañas de serie; interfaz nativa ligera; sin servicios de Google | Parches de seguridad de Chromium con semanas de retraso | **Elegido** |
| CEF (C++) | Parches casi al día; control fino | Mucho más trabajo de interfaz y perfiles; avance más lento | Descartado para v1; posible migración futura |
| WebView2 + interfaz nativa | Muy ligero; parcheado por Microsoft | Solo Windows; dependencia y telemetría de Microsoft; menos control | Descartado (el usuario no descarta Linux) |
| Fork de Firefox | Máxima privacidad; uBlock Origin; muchas funciones hechas | Menor rendimiento que Chromium; rebase cada 4 semanas; compilación pesada | Descartado: el rendimiento es lo que distingue a Sora |
| Electron / Tauri | Rápido de prototipar | Capas extra que restan rendimiento o control | Descartado |
| Motor propio | — | Proyecto de décadas; solo es más rápido mientras hace menos | Descartado |

Elegido por el usuario: **Qt WebEngine**. Bloqueador: **adblock-rust de Brave** (decisión del usuario).

## 5. Decisiones del usuario
- Estética **Niebla**: pestañas en la barra de título, botones minimizar/maximizar/cerrar
  a la **derecha**, debajo barra de herramientas (atrás, adelante, recargar, dirección
  redondeada con candado, escudo con contador de bloqueos, estrella de marcador, menú),
  debajo barra de favoritos. Colores planos gris azulado, sin transparencias.
  Boceto de referencia: https://claude.ai/artifact/JcdkoVfAPYLPF1npdeXcAa (sección E).
- Motor: Qt 6 + Qt WebEngine en C++.
- Plataformas: Windows primero, Linux también.
- Bloqueo con el motor de Brave (adblock-rust).
- Todas las funciones listadas en los objetivos.

## 6. Arquitectura

### Pila técnica (decisiones del constructor)
- **C++20**, **CMake**, **Qt 6.8 LTS** (o la última 6.x estable con Qt WebEngine), módulos
  Widgets, WebEngineWidgets, Sql, Network, Test.
- **Qt Widgets** para la interfaz (menos memoria que QML; no necesitamos animaciones complejas).
- **Rust** (estable) solo para el puente con adblock-rust, compilado como biblioteca
  estática con interfaz C e integrado en CMake con **Corrosion**.
- **SQLite** (vía Qt Sql) para historial y marcadores.

### Módulos

```
src/
  app/        main.cpp, Application (perfiles, ajustes globales, flags de Chromium)
  ui/         MainWindow, TitleBar (+ WindowControls), TabStrip, Toolbar,
              Omnibox (+ sugerencias), ShieldButton, BookmarksBar,
              DownloadsPanel, HistoryView, SettingsDialog, Theme
  browser/    Tab (QWebEngineView + estado), TabManager, TabSleeper,
              ProfileManager (normal / privado), DownloadManager
  data/       Database, HistoryStore, BookmarkStore
  privacy/    AdblockService, RequestInterceptor, CosmeticFilter,
              FilterListUpdater, HttpsUpgrader
adblock-ffi/  crate Rust: envoltorio C sobre adblock-rust
tests/        pruebas con Qt Test
bench/        scripts de medición de arranque y memoria
```

### Flujo de una petición
1. Chromium emite la petición → `RequestInterceptor::interceptRequest` (hilo de E/S).
2. `HttpsUpgrader` reescribe `http://` → `https://` (modo solo HTTPS, con excepción por sitio).
3. `AdblockService::check(url, origen, tipo)` → si bloquea, `info.block(true)` y se
   incrementa el contador de la pestaña (notificado al hilo de interfaz por señal en cola).
4. Al terminar de cargar, `CosmeticFilter` inyecta la hoja de estilos de ocultación
   obtenida de adblock-rust para ese dominio.

`AdblockService` debe ser seguro entre hilos y responder en microsegundos; el motor se
deserializa desde caché en disco al arrancar, en segundo plano, sin bloquear la ventana.

### Ventana Niebla
- Ventana sin marco (`Qt::FramelessWindowHint`) con barra de título propia.
- Windows: gestionar `WM_NCHITTEST` para arrastre, redimensión por bordes y
  **Snap Layouts** de Windows 11 al pasar sobre maximizar (`HTMAXBUTTON`); mantener sombra y
  animaciones nativas (DWM).
- Linux: `QWindow::startSystemMove/startSystemResize`.
- Cerrar en rojo al pasar el ratón, como en Windows.

### Pestañas dormidas
- Tras N minutos sin verse (por defecto 5): `LifecycleState::Frozen`.
- Tras M minutos (por defecto 30) o con presión de memoria: `LifecycleState::Discarded`.
- Nunca se duermen pestañas que reproducen audio, tienen formularios sin enviar o están fijadas.
- Las pestañas restauradas al arrancar empiezan descartadas y solo se cargan al seleccionarlas.

### Privacidad y seguridad por defecto
- Buscador por defecto: **DuckDuckGo** (seleccionable: Brave Search, Startpage, Google…).
- Cookies de terceros bloqueadas (`QWebEngineCookieStore::setCookieFilter`).
- Modo solo HTTPS con página de aviso si el sitio no lo admite.
- DNS sobre HTTPS configurable (`QWebEngineGlobalSettings::setDnsMode`), desactivado por defecto.
- WebRTC limitado a interfaces públicas (no filtra la IP local).
- Modo privado: perfil "off-the-record"; nada se escribe en disco; ventana con color distintivo.
- Sandbox de Chromium siempre activado.
- Sin telemetría. Única conexión de fondo propia: actualización de listas de bloqueo
  (por defecto cada 24 h: EasyList, EasyPrivacy, uBlock filters, uBlock badware).
- Política de actualización *(decisión del constructor)*: con Qt de código abierto, los
  parches de una versión LTS pasan a ser solo comerciales con el tiempo, así que se sigue
  **la última versión menor estable de Qt 6.x**, no 6.8 LTS. Cada versión de Qt con
  correcciones de Chromium implica actualizar, recompilar en CI y publicar una release.
- **Escudo** *(decisión del constructor)*: al pulsarlo muestra el detalle de lo bloqueado y
  dos interruptores por sitio: bloqueador y bloqueo de cookies de terceros. Las excepciones
  se guardan en SQLite y se consultan en memoria (no en disco) en cada petición.

## 7. Rendimiento: cómo lo medimos

Objetivos medidos en el mismo equipo contra Chrome estable, con perfil limpio:

| Métrica | Objetivo v1 |
|---|---|
| Arranque en frío hasta ventana visible | ≤ Chrome |
| Memoria total con 1 pestaña en blanco | ≤ Chrome |
| Memoria con 20 pestañas reales tras 30 min | claramente < Chrome (por pestañas dormidas) |
| Coste del bloqueador por petición | < 50 µs de media |
| Datos descargados en una web con anuncios | < Chrome (por bloqueo) |

`bench/` incluye scripts reproducibles para estas mediciones.

*(Decisión del constructor)* Son objetivos de diseño, no compuertas que bloqueen la v1.
Pero el riesgo se comprueba pronto: el primer hito es un prototipo mínimo (ventana Qt
WebEngine + interceptor con adblock-rust) que el usuario mide en su PC con Windows frente a
Chrome. Se repite la medición en cada hito. Si Qt WebEngine no alcanzara el arranque de
Chrome, se reevalúa con el usuario (recortar, aceptar o considerar CEF).
El arranque en frío se mide sin restaurar sesión.

## 8. Calidad y entrega
- Pruebas unitarias con Qt Test para: almacenes de datos, análisis de la barra de
  direcciones (URL vs búsqueda), puente con adblock-rust, HTTPS upgrader, reglas de TabSleeper.
- GitHub Actions: compilación y pruebas en Linux y Windows; artefacto zip portable para
  Windows generado con `windeployqt`.

## 9. Lo que queda a criterio del constructor
- Estructura interna de clases, señales y nombres más allá de los módulos de §6.
- Esquema exacto de las tablas SQLite.
- Atajos de teclado (seguir los de Chrome/Firefox: Ctrl+T, Ctrl+W, Ctrl+L, Ctrl+Shift+T…).
- Paleta exacta de Niebla en claro y oscuro, partiendo del boceto.
- Orden de implementación, siempre que el navegador sea usable lo antes posible
  (ventana + pestañas + dirección primero).
