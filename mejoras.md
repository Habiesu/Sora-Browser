# Mejoras pendientes

Lista viva de cosas vistas al trabajar en Sora que no son urgentes. Al hacer una, se borra de aquí.

## Rendimiento

- **Chrome carga la página antes que Sora** (medido con `bench.ps1`: ~800 frente a ~720 ms el 20/11/2026 y
  ~900 frente a ~810 ms el 21/11/2026; el arranque y la memoria sí favorecen a Sora). Sospechosos, por
  orden: el `FilterListStore` leyendo y compilando las listas de bloqueo a la vez que carga la
  primera página (compite por CPU), el interceptor en cada petición y el perfil de Sora con caché
  frente al de Chrome. Para saberlo: medir con las listas desactivadas y con el interceptor vacío.
- **El arranque subió ~40 ms con la interfaz Niebla** (Hito 1: 209 ms; Hito 2: 251 ms; Chrome
  247 → 267 ms el mismo día). Sigue por debajo de Chrome. Candidatos: aplicar la hoja de estilos
  de toda la ventana antes de `show()` y crear los iconos al arrancar. Medir con `sora --bench`.

## Limitaciones conocidas

- **El tema elegido en Ajustes no cambia al momento las páginas abiertas**: la ventana sí, pero las
  webs y las páginas `sora://` toman el tema nuevo al recargar (Chrome lo aplica al instante).
- **La lista de descargas se pierde al cerrar Sora** (vive en memoria; guardarla en `sora.db`).
- **La barra libre para arrastrar la ventana es estrecha** con muchas pestañas (entre «+» y
  minimizar); Chrome deja siempre un hueco mínimo.
- **El botón «+» no sigue a las pestañas al arrastrarlas**: mientras se arrastra una pestaña, el «+»
  se queda quieto en su sitio; solo se recoloca junto a la última pestaña al abrir otra. En Chrome
  va siempre pegado a la última pestaña, también durante el arrastre.
- **El icono del puzle** se parece a una nube a 16 px; redibujarlo.
- **Al cambiar a otra pestaña desde la barra**, la nueva pestaña vacía se queda abierta (Chrome la
  cierra).

- **Los bloqueos se reparten por sitio** (el perfil no sabe de qué pestaña viene cada petición):
  con el mismo sitio en dos pestañas de fondo, se los lleva la primera.
- **Instaladores 0.1.0 antiguos** no comprueban la versión: si se ejecuta uno de esos, aún puede
  instalarse encima de una más nueva. Los de 0.2.0 en adelante, no.

## Sin probar

- **Linux:** la ventana sin marco (franja de 4 px para redimensionar, `startSystemMove`) no se ha
  compilado ni probado; solo hay Windows en este PC.
- **A mano en Windows:** el panel de Snap Layouts al pasar sobre maximizar y redimensionar desde
  los bordes. (Arrastrar, doble clic en el borde superior, menú con clic derecho y Win+flechas ya
  se probaron el 21/11/2026.)
- **YouTube:** probado con un vídeo sin anuncio ni aviso antibloqueo. Las reglas son propias
  (`resources/adblock/youtube.js`); si YouTube cambia, actualizarlas o pasar a los scriptlets de
  uBlock Origin desde adblock-rust (`use_resources`) cuando haya una fuente descargable.

## Compilación en Windows

- **Qt publica WebEngine recompilado en subcarpetas numeradas** (`6120/61400/`). `fetch-qt.py`
  elige la más reciente, pero si Qt publica un qtbase nuevo sin recompilar WebEngine, las pruebas
  fallarán con `0xc0000139` (falta un símbolo de Qt en la DLL). Si pasa, la causa es esa.
