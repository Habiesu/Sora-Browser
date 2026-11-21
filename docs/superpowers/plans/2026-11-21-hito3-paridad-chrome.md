# Hito 3 (ampliado) — Paridad con Chrome: plan

> Ejecución inline (superpowers:executing-plans): pruebas primero en cada tarea, commit por tarea.

**Goal:** Que Sora se use como Chrome en lo diario: pestañas y ventanas que se comportan igual,
página de nueva pestaña, menú principal completo, historial, marcadores, descargas, configuración,
extensiones, predicción en la barra de direcciones y bloqueo de anuncios de YouTube.

**Origen:** pruebas del usuario tras el Hito 2 (20-21/11/2026). Decisiones del usuario:
construir ya Configuración, Extensiones, Historial, Descargas y Marcadores (adelanta Hito 3, parte
del 6 y las extensiones); predicción con sugerencias de DuckDuckGo + pestañas (+ historial).

**Spec:** `docs/superpowers/specs/2026-11-18-sora-browser-design.md` (§2 objetivos, §6 módulos
`browser/` y `data/`, §7 rendimiento). Referencia de comportamiento: Chrome y Brave en Windows 11.

## Global Constraints
- C++20, Qt Widgets para la ventana (barras, pestañas, menús). Textos en español.
- **Ruling (avisado al usuario):** las páginas internas (nueva pestaña, historial, descargas,
  marcadores, configuración, extensiones) son HTML servido por el esquema `sora://` con
  `QWebEngineUrlSchemeHandler` y `QWebChannel`, como hace Chrome. Solo ocupan memoria abiertas; la
  ventana sigue en Widgets. Sin recursos externos (todo en `resources/`).
- Datos en SQLite (Qt Sql) en `AppLocalDataLocation`; ajustes en `QSettings`. Nada de esto se lee
  antes de `window.show()`.
- Incógnito = `QWebEngineProfile` sin nombre (no guarda nada); no se graba historial ni descargas.
- Arranque y memoria con una pestaña siguen ≤ Chrome (`bench.ps1`); si no, parar y avisar.

## Tareas

1. **Cierre con WebGL (github.com).** Causa en Chromium dentro del proceso (GPU en proceso). Pila
   con símbolos → arreglo o mitigación mínima con su prueba.
2. **Pestañas como Chrome.** Título a la izquierda también sin icono; × centrada en vertical;
   separadores entre pestañas inactivas; fondo de la vista del tema mientras carga (no blanco);
   arrastrar para reordenar sin saltos; arrastrar fuera crea ventana nueva; soltar sobre otra
   ventana la mueve allí.
3. **Varias ventanas e incógnito.** `Browser` (dueño de perfiles y ventanas): Ctrl+N, Ctrl+Shift+N
   (ventana con perfil sin disco, barra de título más oscura e icono de incógnito), cerrar la
   última ventana sale; `SingleInstance` abre en la última ventana activa.
4. **Ventana como Chrome.** Comprobar y ajustar: doble clic en borde superior (estirar en vertical),
   arrastrar al borde superior (maximizar), a los lados (mitad), Win+flechas, menú de la barra con
   clic derecho, maximizada sin bordes, pestañas en el borde superior al maximizar.
5. **Nueva pestaña (`sora://newtab`).** Texto breve centrado, caja de búsqueda y accesos a los
   sitios más visitados (cuando haya historial). Título "Nueva pestaña", sin URL en la barra.
6. **Menú principal** como Chrome/Brave: Nueva pestaña, Nueva ventana, Nueva ventana de incógnito ·
   Historial, Descargas, Marcadores (marcar página, mostrar barra, administrar) · Zoom (−, %, +,
   pantalla completa) · Imprimir, Buscar en la página, Guardar página como · Más herramientas
   (Borrar datos de navegación, Herramientas para desarrolladores) · Extensiones, Configuración,
   Ayuda/Acerca de Sora · Salir. Con sus atajos de Chrome.
7. **Historial** (`HistoryStore`, SQLite): visitas con título y hora; `sora://history` con
   búsqueda, borrar entrada y borrar todo; Ctrl+H.
8. **Marcadores** (`BookmarkStore`, SQLite): estrella funcional (Ctrl+D), barra de favoritos con
   carpeta "Otros", `sora://bookmarks` para ordenar, renombrar y borrar; Ctrl+Shift+B / Ctrl+Shift+O.
9. **Descargas** (`DownloadManager`): a Descargas del usuario, botón en la barra con progreso,
   `sora://downloads` (abrir, mostrar en carpeta, cancelar, reintentar); Ctrl+J. Se reactivan
   "Guardar página como" y las opciones de descarga del menú contextual.
10. **Configuración** (`sora://settings`): buscador (DuckDuckGo por defecto, Brave, Google, Bing,
    Startpage), al iniciar (nueva pestaña / restaurar pestañas), tema (sistema/claro/oscuro),
    bloqueador activado, carpeta de descargas y preguntar dónde guardar, borrar datos de
    navegación, acerca de.
11. **Extensiones** (`QWebEngineExtensionManager`): `sora://extensions` con cargar descomprimida,
    instalar desde .zip/.crx, activar/desactivar y quitar; botón de puzle en la barra con las
    ventanas emergentes de cada extensión.
12. **Predicción en la dirección:** lista desplegable con sugerencias de DuckDuckGo
    (`https://duckduckgo.com/ac/`), pestañas abiertas, historial y marcadores; autocompletado en
    línea del dominio más visitado; flechas y Enter como Chrome.
13. **Anuncios de YouTube:** filtros cosméticos y scriptlets de uBlock (adblock-rust: recursos y
    `url_cosmetic_resources`) inyectados antes de cargar cada página.
14. **Medir y cerrar:** `bench.ps1`, resultados en `bench/results/`, CLAUDE.md y mejoras.md.

## Review Focus
1. Incógnito no escribe nada en disco (historial, cookies, caché, descargas registradas).
2. Arrastrar una pestaña con un vídeo a otra ventana no lo para ni lo recarga.
3. Cerrar la última pestaña de una ventana con otras ventanas abiertas no cierra Sora.
4. Páginas `sora://` no son accesibles desde webs normales (no se pueden incrustar ni leer).
5. Sin red: la predicción y las listas fallan en silencio sin bloquear la barra.
