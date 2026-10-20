# Caso 04: cobertura y plan mínimo de regresiones de Misku

Revisión: 3–4 de octubre de 2026 (America/Bogota). Revisión estática del worktree `C:\Users\johan\.codex\worktrees\misku-ux-regressions\misku-terminal`.

HEAD comprobado: `4aa01e3f8bcb9845e3216660dbfb595e0c6f5176`.

## Dictamen y alcance

La cobertura existente permite comprobar serialización, cálculo de dimensiones y comportamiento de una página XAML. No acredita movimiento, captura, hit testing ni Snap del HWND de escritorio. La prioridad es validar el contrato integrado de los casos 01/02 con el temporizador de ocultación y con la región nativa de entrada; ampliar únicamente las aserciones de altura no cerraría esa brecha.

Esta ejecución solo inspeccionó fuentes, proyectos, selección de CI y documentación. No compiló, ejecutó tests, instaló ni inició Misku; tampoco abrió un host GUI alternativo. La restricción de validación gráfica sigue vigente. Todos los tests y mediciones de este informe son propuestas, salvo las coberturas verificadas por lectura y las cifras expresamente históricas.

En `doc/misku/cases/` solo estaba `04-ux-validation.md` al iniciar la revisión. Los documentos 01/02 no están disponibles en este worktree. La matriz cubre las interacciones solicitadas de barra y ventana a partir del código real; no presupone sus soluciones ni afirma revisar cambios futuros. La validación integrada requiere las implementaciones de 01+02, su contrato definitivo y un nuevo HEAD identificado. No se inspeccionaron ni modificaron los otros worktrees.

## Cobertura existente verificada

Las referencias `archivo:línea` corresponden al HEAD anterior, salvo el documento local del caso. Las cantidades de pruebas aprobadas de septiembre no se cuentan como resultados de esta revisión.

| Capa | Evidencia en el repositorio | Qué demuestra el test por su diseño | Límite |
| --- | --- | --- | --- |
| Configuración/modelo | `src/cascadia/UnitTests_SettingsModel/ApplicationStateTests.cpp:141,160` | Round trip de nombres, pertenencia y sesión activa; tamaño de cliente 1200×800 durante cinco round trips, sidebar en ambos estados, reconocimiento de formato antiguo | No recrea cinco ventanas ni verifica foco, pin persistido en otro proceso o DPI físico |
| Configuración/atajos | `src/cascadia/UnitTests_SettingsModel/MiskuConfigTests.cpp:37,85` | Alias resueltos a acciones; overlay válido/inválido y eliminación | No envía teclas a la shell ni ejercita el watcher de escritorio |
| Dimensiones sin GUI | `src/cascadia/LocalTests_TerminalApp/CommandlineTest.cpp:113` | `GetLaunchDimensions` a 96/144 DPI, sidebar visible/oculto, cinco serializaciones y formato legado | DPI es un argumento; no cambia un monitor. El legado usa el estado de pin ambiental, sin parametrizar ambos valores |
| XAML UAP/TestHost | `src/cascadia/LocalTests_TerminalApp/TabTests.cpp:60,220` | Atajos mediante `TryKeyChord`, sidebar y asignación de pestañas; layout de sesión | No verifica ruta física de teclado. Este test fuerza `ShowTabsInTitlebar(false)` |
| Barra aislada | `TabTests.cpp:343` | `TitlebarControl{uint64_t{0}}`: arranque con pin false/true, visibilidad/altura 3/40 DIP y llamada directa a hover nativo true | No monta esta barra en el árbol visual; no hace hover false ni bombea el temporizador; no pin por clic, HWND real, captura o fila reservada |
| Sesiones/restauración XAML | `TabTests.cpp:407` | Mover/cerrar una pestaña, aislamiento visible y crear una página nueva desde acciones persistidas; sesión activa y sidebar oculto | No es un reinicio del proceso. `GetActiveControl()!=null` no acredita foco de teclado ni recepción de entrada |
| Arranque XAML | `TabTests.cpp:394` | Sin acciones explícitas: una pestaña, control activo y contenido con tamaño positivo | No verifica activación del HWND ni shell funcional |
| Colores/selección | `TabTests.cpp:363` | Colores actuales, reutilización de brush y recuperación de la pestaña seleccionada al regresar a una sesión | Aserción de identidad es útil para el diagnóstico de asignaciones, pero no demuestra fluidez |
| Benchmark XAML | `TabTests.cpp:291` | Registra tiempos de switch y `UpdateLayout`, conserva un item de sidebar y exige control activo | No hay umbral de tiempo ni aserción de presentación de frames |
| CI configurado | `.github/workflows/misku-ci-cd.yml:159` y `tools/misku/Test-Misku.ps1:69` | Selecciona SettingsModel y CommandlineTest; sube logs | Invoca sin `-UI`, por lo que no ejecuta TabTests. Compilar su biblioteca/TestHost no equivale a ejecutar sus pruebas |

`Test-Misku.ps1:49–65` rechaza código de salida no cero, selección vacía, fallo, bloqueo, Not Run o Skipped. Es una protección útil que debe conservarse. No detecta que una categoría nunca fue seleccionada: sin `-UI`, el mensaje final de éxito no acredita XAML.

El proyecto `TerminalApp.LocalTests.vcxproj:53–65` referencia TerminalAppLib y sus dependencias, sin WindowsTerminal/NonClientIslandWindow. La búsqueda en los archivos de tests de Cascadia encontró la prueba aislada de TitlebarControl, pero no una prueba dedicada del input sink nativo. La ausencia afirmada se limita a esa búsqueda y a esa selección de CI.

## Brechas por prioridad

### P0 — Falta una prueba integrada de temporizador, arrastre y región nativa

Evidencia: `TitlebarControl.cpp:20–40,144–217` aplica 180 ms y decide visibilidad con pin, hover XAML, hover nativo y foco de teclado. `TitlebarControl.h:58–65` no incorpora estado de movimiento/captura. `NonClientIslandWindow.cpp:188–263` activa/desactiva hover por mensajes de movimiento/salida y reenvía el inicio del arrastre al padre. La revisión no encontró manejo explícito de `WM_ENTERSIZEMOVE`, `WM_EXITSIZEMOVE`, `WM_CAPTURECHANGED` o `WM_CANCELMODE` en WindowsTerminal.

Riesgo a reproducir, no fallo dinámico confirmado: al salir de la región con un arrastre activo, la ocultación puede cambiar el área de entrada antes de terminar el gesto. `NonClientIslandWindow.cpp:319–350,464–509` reposiciona el input sink mediante SetWindowPos y distingue región colapsada y expandida. Los callbacks de SizeChanged enlazan XAML y HWND. Ningún test actual ejecuta esa cadena durante el bucle nativo de movimiento.

Propuesta mínima: contrato de política sobre interacción activa si 01/02 lo introducen, test XAML del timer real y escenario de HWND real con arrastre de 600 ms, salida de región, pausa y release/cancelación. Criterio: cero ocultaciones/cambios incompatibles de hit target durante el gesto, una finalización, ausencia de captura retenida después, y ocultación posterior solo si no queda ninguna causa de visibilidad. No inferir captura exclusivamente de `GetCapture()`: registrar también el estado/mensajes del bucle nativo y el HWND dueño de la entrada.

### P1 — Hover, pin, foco y menús carecen de secuencias completas

Evidencia: el test de barra termina después de `SetNonClientPointerOver(true)` (`TabTests.cpp:356`). No comprueba el regreso a oculto, cancelación de un tick pendiente, cruce XAML/native o destrucción con timer activo. `TitlebarControl.cpp:93–115,158–173` guarda pin, limpia estados al desactivar y distingue foco Keyboard de Pointer. `NonClientIslandWindow.cpp:986–1011` conecta activación nativa y menú de sistema. No hay estado explícito de flyout abierto en la política actual.

Hipótesis pendientes: desactivar, transitar entre controles o abrir un popup puede limpiar hover/foco en un orden distinto del esperado. La inspección no confirma pérdida de foco ni cierre de menú.

Propuesta mínima: eventos reales en árbol XAML, pin mediante ToggleButton y observación del contenido reservado; menú abierto más allá de 180 ms y salida con Escape; Alt+Tab de ida/vuelta. Criterio: ninguna acción perdida/duplicada, ningún botón invisible activable, foco final en el destino previsto y pin respetado. Para un menú abierto no exigir necesariamente barra visible: exigir menú estable, acción accesible y restauración de foco; el contrato de 01/02 debe decidir la política visual.

### P1 — No existe cobertura nativa de bordes, Snap, maximización o DPI

Evidencia: `_dragBarNcHitTest` (`NonClientIslandWindow.cpp:107–155`) devuelve HTTOP/HTCAPTION con barra oculta y HTMINBUTTON/HTMAXBUTTON/HTCLOSE cuando está visible. `_GetDragAreaRect:464` transforma DIP a píxeles. `_UpdateIslandPosition:582–621` aplica un desplazamiento de un píxel al maximizar y actualiza la región. Son rutas ajenas al TitlebarControl con handle cero.

Propuesta mínima: mapa de hit testing del HWND correcto, puntos en límites y acciones reales de resize/doble clic; prueba física separada de Snap y traslado de monitor. Criterio: concordancia entre objetivo visible, mensaje HT* y acción; región actualizada a DPI/estado de ventana, sin franja nativa que robe clics al contenido. Un HTMAXBUTTON correcto es requisito para Snap, pero no prueba que el popup de Snap se presente.

### P1 — Foco real y restauración conjunta de sesiones/barra están incompletos

Evidencia: `_initializeTerminalPage` monta/activa la página, espera Initialized y, con acciones por defecto, fuerza selección de la primera pestaña (`TabTests.cpp:484–574`). Sin acciones por defecto retorna antes de esa corrección; las pruebas de arranque/restauración aprovechan esa ruta y son más representativas. Aun así no consultan FocusManager ni comprueban entrada al control esperado. `ProcessStartupActions` cede al dispatcher entre acciones (`TerminalPage.cpp:786–811`); `_CompleteInitialization:856–868` restituye la sesión activa al final.

Propuesta mínima: ampliar la restauración real de página con varias pestañas por sesión, sesión guardada distinta de la última creada y foco comprobado tras completar el dispatcher; ejecutar ambas rutas de tabs en titlebar y fuera de ella. Parametrizar pin en dimensiones; cinco reinicios reales quedan para escritorio.

Criterio: pestañas y pertenencia exactas, sesión guardada seleccionada, contenido visible con tamaño positivo y destino de foco correcto; geometría sin reducción acumulativa y estado de pin conservado. Distinguir selección lógica, foco XAML, activación del HWND y llegada de texto a ConPTY como cuatro evidencias distintas.

### P1 — El benchmark detecta tiempos, pero no regresiones de latencia

Evidencia: `TabTests.cpp:291–339` registra percentiles sin aserciones temporales, conserva la identidad de un item y comprueba control activo. `_commonSetup:841–874` usa tabs fuera del titlebar e historiales mínimos. Se crean dos sesiones y las pestañas adicionales se abren en la segunda: las cargas son 1/1, 1/9 y 1/49, no balanceadas.

La sesión seleccionada tras crear las pestañas es la segunda. El primer intento de `after_open` también elige la segunda (`iteration == 0`); `_SwitchToSession` retorna inmediatamente si ya está activa (`TabManagement.cpp:240–243`). Esa fase mezcla una llamada sin cambio con 29 cambios reales, aunque su UpdateLayout aún puede costar tiempo. `repeat` comienza tras terminar en la primera sesión y sí alterna 30 cambios. Esto se deduce del flujo; no es una medición nueva.

Además, el cronómetro empieza dentro del trabajo despachado al hilo UI: no incluye espera previa en su cola. `UpdateLayout` no espera presentación a pantalla. Los datos se ordenan sin conservar la secuencia temporal de los picos. La cobertura permite localizar trabajo síncrono de layout, pero no acreditar latencia percibida, FPS ni ausencia de tirones.

Propuesta mínima: conservar este escenario como referencia; añadir datos crudos ordenados, dirección y etiqueta de no-op, una fase de cambios efectivos y comparación pareada con baseline actual en la misma máquina. No sustituir este diagnóstico por rediseño de sesiones. Solo ampliar a carga equilibrada o salida de shell si la primera comparación deja dudas.

## Qué puede verificar cada capa

| Capa propuesta | Evidencia válida | No demuestra | Condición de ejecución futura |
| --- | --- | --- | --- |
| Tests de política sin UI | Decisiones temporales y transiciones con reloj controlado, cancelación de ocultación y finalización de gesto | Eventos XAML, región HWND o frames | Usar la política real que 01/02 expongan. No crear una copia de la lógica solo para que el test pase |
| Modelo/configuración | Serialización y dimensiones con DPI parametrizado, aliases, pin persistido en almacenamiento aislado | Watcher, proceso de escritorio ni monitores | Suites actuales ampliadas mínimamente; aislamiento del estado y restauración en cleanup |
| XAML TestHost UAP | Árbol visual, eventos de foco/puntero, DispatcherTimer real, tamaño reservado y layout de página | NonClientIslandWindow, bucle de movimiento del sistema ni ConPTY de escritorio | Runner con dependencias y ejecución UI permitida; esta tarea no lo ejecuta |
| Harness nativo con HWND real | Input sink de producción, WM_NCHITTEST, rectángulos, reenvío y estado del bucle de movimiento | Experiencia física de Snap, todos los monitores o presentación de frames | Diseñar como escenario distinto; no ejecutar un sustituto GUI para eludir la política vigente |
| Validación física de escritorio | Gestos, Snap real, menús, shell, cambio de monitor, foco y evidencias visuales | Distribución estadística sin instrumentación | Pendiente mientras la política impida iniciar Misku; no se marca aprobada por tests de capas anteriores |

Un test de política debe comprobar invariantes del producto (no perder el gesto, no ocultar un control enfocado, ocultar tras inactividad), no una tabla que reproduzca exactamente el OR privado. Las dimensiones 3/40 DIP son un contrato existente verificable; la identidad de brushes/items sigue siendo solo una protección de la optimización actual.

## Conjunto mínimo de regresiones a implementar después de 01+02

Son seis grupos parametrizados; los nombres son propuestas y no existen todavía. No se añaden tests ni cambios de producción en esta tarea.

1. **ChromeInteractionPolicy** (sin UI, si hay política real comprobable). Entrada/salida de cada fuente; handoff XAML→native y native→XAML; reentrada antes del vencimiento; pin durante ocultación pendiente; interacción de arrastre/resize, cancelación y pérdida de captura. Reloj controlado a 179/180/181 ms para el contrato actual. Resultado: cero ocultaciones con causa activa; como máximo una ocultación válida después del plazo; ninguna acción de timer obsoleto tras reentrada o destrucción.
2. **ChromeDispatcherAndLayout** (XAML). Montar TitlebarControl en un host, ejercitar enter/exit y foco Keyboard/Pointer por eventos, timer real, toggle por clic y liberar el control con callback pendiente. Medir `ChromeRoot` visible/hit-test y posición/tamaño del contenido: hover sin pin no altera su espacio reservado; pin lo altera una vez. Un host de TitlebarControl solo no valida la fila de NonClientIslandWindow: esa aserción pertenece al harness nativo. No bloquear el hilo UI con Sleep ni llamar directamente al handler Tick para simular la integración.
3. **NativeChromeWindowInteraction** (HWND). Input sink de producción, geometría y hit testing con barra oculta/revelada/fijada; arrastre y resize 600 ms, salida de región, release/Escape/desactivación; doble clic y max/restauración. Sintetizar WM_NCHITTEST sirve para el mapa, pero enviar solo WM_NCLBUTTONDOWN/UP no reproduce un gesto nativo completo. Este grupo necesita entrada real y bombeo de mensajes en un escritorio permitido.
4. **SessionFocusAndRestore** (XAML más complemento físico). Ampliar la prueba existente con tres pestañas en dos sesiones, sesión activa distinta de la última acción, cambio por atajo y sidebar, mover/cerrar y restaurar nueva página. No corregir selección desde el fixture. Esperar Initialized y trabajo asíncrono de foco con deadline; comprobar FocusManager/descendiente esperado, no solo puntero no nulo. Probar tabs dentro/fuera de titlebar. En escritorio, texto marcador va exactamente a la shell activa y cero a la inactiva; menús/Escape/Alt+Tab devuelven el foco.
5. **RestoredDimensionsAndPin** (modelo/comandos y complemento HWND). Parametrizar pin false/true en lugar de depender del singleton; sidebar false/true, 96/144 DPI, formato cliente/legado. Aislar almacenamiento de ApplicationState. No repetir un round trip cubierto: añadir las combinaciones faltantes y el pin a través de guardar/cargar estado. Cinco ciclos reales de cierre/reapertura y cambio de DPI verifican que cálculo, guardado y ventana coinciden.
6. **SessionSwitchRegressionMeasurement** (XAML, diagnóstico; escritorio para percepción). Preservar 2/10/50 pestañas, `after_open`/`repeat` y métricas separadas switch/layout. Registrar no-op por separado y 30 cambios efectivos para la comparación corregida, sin mezclarlo con el baseline antiguo. Añadir aserción funcional de sesión y selección destino tras cada cambio. El gate de latencia debe comparar ejecuciones actuales controladas, no un número histórico aislado.

En todos: limpiar pin/estado/handlers aunque falle el test; fixtures sin ajustes personales; timeouts se reportan como fallo o bloqueo según causa, nunca como pass. Infraestructura sin manifiesto/dependencias es bloqueo y no evidencia de regresión del producto ni de éxito.

## Matriz de validación y criterios medibles

Todo está **pendiente de ejecución**. Parámetros base: Windows/build, commit/artifact hash, WinUI efectivo, máquina, resolución/Hz, DPI, tabs en titlebar sí/no y sidebar sí/no registrados. Usar barra sin pin, fijada y revelada; ventana normal y maximizada. Geometría en DIP para XAML y píxeles para HWND, con tolerancia máxima de 1 píxel físico por redondeo. No confundir ese margen con permiso para perder una franja de hit testing.

| Escenario | Capa principal | Procedimiento mínimo | Criterio de aceptación |
| --- | --- | --- | --- |
| Hover XAML | XAML + física | Entrar/salir del tab row y pin; cruzar 20 veces | Revelación consistente; cero botones visibles sin hit target; al salir sin otra causa, una ocultación posterior al plazo |
| Hover native y handoff | Política + XAML + HWND | Drag region/botones→tabs y vuelta; 20 cruces, reentrada a 100 ms | Cero ocultaciones durante presencia continua; tick antiguo no oculta tras reentrada; hit target final coincide con visual |
| Arrastre >180 ms | HWND + física | Mantener 600 ms, salir de la barra con botón pulsado, detenerse 300 ms, soltar; 10 repeticiones por estado | Movimiento termina una sola vez; cero colapsos de chrome durante interacción activa; hit region estable respecto al cliente salvo ajuste esperado de DPI/estado; cero captura/interacción retenida |
| Cancelar gesto | HWND + física | Arrastre/resize, Escape y desactivación; 10 por ruta | Fin registrado, ventana utilizable, botones sin estado pressed residual; ocultación posterior según contrato |
| Pin en ejecución/arranque | Modelo + XAML + HWND | 10 toggles, incluyendo hide pendiente; guardar/cargar aislado y cinco reinicios reales cuando se permita | Pin conserva visibilidad; espacio reservado pasa 3↔40 DIP una vez por toggle, diferencia 37 DIP en modo normal con título; hover sin pin no cambia tamaño reservado; valor persiste |
| Resize | HWND + física | Borde superior y esquinas, widths estrechos, 600 ms; pin on/off | Bordes arrojan HT apropiado; gesto continuo; contenido y región se ajustan, cero activación accidental de caption button |
| Doble clic | HWND + física | Drag region, 10 ciclos normal→max→restore; control negativo sobre pin/tab | Una transición por doble clic válido; rect de restauración consistente; pin/tab no dispara maximización |
| Snap | HWND + física | Hover de maximize y selección de layout; Win+Z/Win+flechas si están habilitados en ese Windows | HTMAXBUTTON en botón visible, nunca sobre botón oculto; popup real accesible, layout elegido aplicado y chrome usable. Si Snap está deshabilitado, documentar configuración y marcar ruta no ejercitada |
| Foco de teclado y ratón | XAML + HWND + física | Tab entre controles, clic, Ctrl+B, sesión por atajo/sidebar, Alt+Tab; texto marcador | Keyboard mantiene controles accesibles; Pointer no deja barra fijada accidentalmente; foco/entrada al destino esperado, cero texto en sesión inactiva |
| Menús/flyouts/sistema | XAML + HWND + física | Abrir menú, sacar puntero de barra, esperar 600 ms, seleccionar o Escape | Menú permanece operable; una acción o cancelación; foco vuelve al destino previsto; sin click-through al terminal |
| Monitor/DPI | HWND + física | Monitores 100/150%; 125/200% si disponibles; traslado ida/vuelta, monitor con coordenada negativa si existe | DPI registrado cambia como corresponde; botones y reveal corresponden a hit map y bordes, tolerancia 1 px; no targets desplazados. Simular un DPI numérico no aprueba este escenario |
| Ventana maximizada | HWND + física | Hover/clic en primera fila de píxeles, reveal, drag para restaurar y doble clic | Controles alcanzables en borde superior; sin fila muerta ni resize impropio en maximizada; mapa/rect correcto después de restaurar |
| Sesiones y restauración | Modelo + XAML + física | Mover/cerrar, cambiar sesión, guardar/recrear; cinco ciclos de proceso con sidebar/pin ambos estados | Conteos/pertenencia/sesión activa exactos; foco comprobado y tamaños positivos; sin deriva acumulativa de cliente normalizada por DPI (≤1 px por comparación) |
| Modo foco/fullscreen | XAML + HWND + física | Entrar/salir con pin on/off, tabs fullscreen on/off | Visibilidad y región siguen modo; sin input sink invisible que capture clics; pin reaparece correctamente al volver |
| Navegación con 50 pestañas | XAML + medición física separada | Fases y sentidos del protocolo siguiente | Sesión/foco correctos en todas las muestras; tiempos crudos completos, regresión evaluada contra baseline contemporáneo; ninguna afirmación de FPS sin capturas de presentación |

Tiempo: 180 ms es el plazo actual de política, no garantía de callback puntual durante el bucle modal de Windows. En tests con reloj controlado, verificar el límite exacto. En XAML real, después de salida esperar de forma asíncrona hasta 1 s como deadline funcional propuesto; registrar el tiempo real y no usar ese segundo como presupuesto de rendimiento. Para reentrada, exigir continuidad durante otros 600 ms con causa activa. El fixture debe demostrar que el dispatcher siguió procesando callbacks; un timer que nunca corre no aprueba la prueba.

Las repeticiones detectan regresiones reproducibles; no certifican todas las combinaciones del hardware. Ejecutar el núcleo en 100% normal/maximizada y repetir los escenarios de mayor riesgo —handoff, arrastre, resize, hit map y Snap— en 150% y durante traslado entre monitores. Marcar combinaciones no disponibles como pendientes con razón, no como aprobadas.

## Rendimiento: evidencia histórica y protocolo propuesto

`doc/misku/validation.md:3,34–47` está fechado el **7 de septiembre de 2026**. Documenta 233 pruebas y medidas históricas; no prueba el HEAD ni el comportamiento actual de escritorio. Para 50 pestañas, su tabla registra:

| Fase histórica | Mediana | P95 | Máximo |
| --- | ---: | ---: | ---: |
| after_open | 13.503 ms | 106.739 ms | 154.974 ms |
| repeat | 18.616 ms | 33.190 ms | 37.247 ms |

El documento también declara un máximo observado entre ejecuciones de **273.756 ms tras abrir 50 pestañas**, y variación en repeat hasta mediana 14.496/P95 16.013 ms. La comparación previa a reutilizar recursos de color (168.187/242.908 ms) era diagnóstica, sin toda la carga del sistema controlada. No permite calcular una mejora actual ni justificar ausencia de tirones. No convertir estos milisegundos en FPS.

Protocolo mínimo futuro:

1. Compilar baseline identificado y resultado de 01+02 con mismo toolchain, configuración Release x64 y runtime; registrar hashes y fecha. Esta tarea no lo hace. No comparar el MSIX histórico/instalado con un DLL recién generado por su número de versión únicamente.
2. Hacer tres pares baseline/candidato alternando el orden, máquina y ajustes iguales, carga del sistema registrada. Recrear página por corrida; reproducir secuencialmente 2→10→50 y fases originales. Conservar 1/49 e historial mínimo para equivalencia. Las muestras de una prueba modificada se comparan usando esa misma prueba en ambas versiones.
3. Guardar por muestra: fase, ordinal, sesión origen/destino, cambio efectivo/no-op, timestamp, tiempo switch, layout y total, resultado de selección/foco. Mantener orden cronológico y datos crudos; calcular percentiles con definición explícita. El test actual usa índices 15 y 28 de 30 valores ordenados para p50/p95; mantenerlos para comparación histórica, o versionar el método si se cambia.
4. Para 50 pestañas comparar por fase y dirección. Criterio provisional de regresión: escalar si el P95 del candidato empeora respecto a su baseline pareado en más de `max(20%, 5 ms)` en al menos dos de tres pares. Es una tolerancia propuesta para este proyecto, no un estándar ni un resultado observado. Baseline ruidoso/inconsistente impide cerrar rendimiento y exige investigar, no aprobar por ausencia de señal.
5. Conservar todos los máximos y cada muestra >100 ms como evento de diagnóstico propuesto; no descartarlos como warmup. Si los picos persisten, recoger una traza UI/CPU/asignaciones para localizar switch, layout o trabajo de inicialización. Un pico de baseline no exime al candidato, pero sí obliga a separar regresión nueva de problema preexistente.
6. Latencia percibida exige otra medición: timestamp de entrada/encolado, ejecución UI y presentación real. Capturar datos de presentación/traza adecuados al renderer y validar su correspondencia con la acción; no tratar UpdateLayout como frame presentado. Si no se dispone de esa instrumentación, reportar únicamente tiempos síncronos y observación física, sin FPS ni latencia teclado→pantalla.

La ruta `_RefreshSessionSidebar` reutiliza items y cuenta pestañas una vez (`TabManagement.cpp:250–305`); `_ApplySessionVisibility:176` visita pestañas y actualiza selección. Son candidatos de instrumentación, no causas demostradas de los picos. No se propone rediseño, virtualización ni cambio de arquitectura en este caso. Historial grande, salida sostenida de shell y memoria quedan como alcance adicional solo si se solicita o si el diagnóstico necesita esa carga; las cifras actuales no los cubren.

## Plan de ejecución mínimo y cierre

1. **Ahora, completado por inspección:** verificar HEAD, mapear tests/runner/CI y separar evidencia de septiembre. Entregar este informe. Producción y tests conservados.
2. **Después de 01+02:** revisar diff y contrato final, anclar nuevo commit, implementar grupos 1/2/3 donde corresponda y ampliaciones 4/5; no duplicar lógica de producto. El grupo 1 solo es viable si usa una unidad real comprobable; de otro modo cubrir contrato en XAML/host, sin añadir una segunda implementación de política.
3. **Validación sin UI:** suites de modelo/comandos, aserciones relevantes y resumen completo del runner. Agregar política a una selección explícita que CI realmente ejecute. Conservar logs con commit/categorías ejecutadas. Verificar comandos estáticamente antes de ejecutarlos; Test-Misku compila por defecto y no es un comando de inspección.
4. **XAML y HWND:** etapa separada en entorno Windows permitido con dependencias verificadas. `-UI` selecciona TabTests, no un test nativo todavía inexistente. No activar GUI en este entorno para superar la restricción. Registrar categorías bloqueadas y responsables del siguiente paso.
5. **Física/rendimiento:** matriz núcleo y variantes de mayor riesgo; tres comparaciones pareadas del grupo 6. Mantener evidencia de configuración independiente de evidencia de interacción y medición.

Cierre funcional: cero fallos de invariantes de los casos 01/02 y de sesiones; selecciones no vacías, sin fallos/bloqueos/omisiones inadvertidos; mapa HWND y gestos físicos confirmados en combinaciones registradas. Cierre de rendimiento: mediciones crudas contemporáneas, sin regresión según el criterio acordado y picos descritos. Si sigue bloqueada la GUI, el estado final será **cobertura estática y pruebas permitidas completadas; validación de escritorio pendiente**, nunca “UX validada”.

No se revalidaron en vivo la versión instalada, el CI remoto ni hashes de los paquetes históricos citados en el documento del caso. Tampoco se requiere publicación, instalación, mensajes a otras tareas ni cambios en otros worktrees para entregar este análisis.
