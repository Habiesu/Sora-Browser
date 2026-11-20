# Mejoras pendientes

Lista viva de cosas vistas al trabajar en Sora que no son urgentes. Al hacer una, se borra de aquí.

## Rendimiento

- **Chrome carga la página antes que Sora** (medido con `bench.ps1` el 20/11/2026: ~800 ms frente
  a ~1000 ms desde el lanzamiento; el arranque y la memoria sí favorecen a Sora). Sospechosos, por
  orden: el `FilterListStore` leyendo y compilando las listas de bloqueo a la vez que carga la
  primera página (compite por CPU), el `RequestInterceptor` en cada petición y el perfil de Sora
  con caché frente al de Chrome. Para saberlo: medir con las listas desactivadas y con el
  interceptor vacío.

- **El arranque subió ~40 ms con la interfaz Niebla** (Hito 1: 209 ms; Hito 2: 251 ms; Chrome
  247 → 267 ms el mismo día). Sigue por debajo de Chrome. Candidatos: aplicar la hoja de estilos
  de toda la ventana antes de `show()` y crear los iconos al arrancar. Medir con `sora --bench`.
- **Esc no para la carga** (en Chrome sí): un atajo global le quitaría Esc a la barra de
  direcciones. Hacerlo en la vista web (tecla en la página) o solo cuando la dirección no tiene
  el foco.
- **Con más pestañas de las que caben**, el "+" se coloca junto a las flechas de desplazamiento y
  tapa en parte la última pestaña visible.

## Sin probar

- **Linux:** la ventana sin marco (franja de 4 px para redimensionar, `startSystemMove`) no se ha
  compilado ni probado; solo hay Windows en este PC.

## Compilación en Windows

- **Qt publica WebEngine recompilado en subcarpetas numeradas** (`6120/61400/`). `fetch-qt.py`
  elige la más reciente, pero si Qt publica un qtbase nuevo sin recompilar WebEngine, las pruebas
  fallarán con `0xc0000139` (falta un símbolo de Qt en la DLL). Si pasa, la causa es esa.
