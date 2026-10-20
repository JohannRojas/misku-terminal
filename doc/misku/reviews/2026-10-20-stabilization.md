# Estabilización de ventana — 20 de octubre de 2026

Se parte de `4aa01e3f8` (PR #4). Los cuatro worktrees de octubre contenían análisis, no implementaciones. Sus revisiones de geometría, autoocultado y cobertura se conservan en esta carpeta.

## Cambios

- Una notificación explícita vincula la visibilidad del chrome con la región nativa de entrada. Los cambios de tamaño del título, pin y botones completan la actualización después del layout.
- La región nativa excluye el botón de fijar, manteniendo la detección nativa de maximizar que utiliza Snap.
- Los botones de ventana se detectan dentro de sus rectángulos visibles, incluyendo altura y estado fullscreen.
- El autoocultado permanece suspendido durante el movimiento o redimensionado nativo. Se protege tanto la programación del temporizador como su callback; el retorno del bucle modal y `WM_EXITSIZEMOVE` liberan ese estado.
- Se añaden pruebas XAML del temporizador real durante una interacción prolongada y de las notificaciones geométricas.
- CI conserva también el MSIX y la dependencia WinUI, además del portable.

## Trabajo preservado aparte

Los 22 archivos del actualizador incompleto se guardaron y publicaron en `wip/misku-updater-stabilization` (`e96d859da`). Ese cambio conserva el error de compilación previamente diagnosticado y no forma parte de esta distribución. La actualización instalada no anuncia ni instala futuras versiones desde la interfaz todavía.

## Validación

La compilación local Release x64 produjo el MSIX 0.2.8.0. Los resultados de las suites y del CI se registran en el PR y en los logs de la ejecución; no deben inferirse de la compilación. Las revisiones del 3 de octubre son evidencia estática e histórica.

El TestHost XAML comprueba controles y temporizadores, no el bucle de movimiento de una ventana de escritorio ni la experiencia física de Snap o de varios monitores. La validación manual de esos escenarios sigue siendo necesaria; no se afirma ausencia de parpadeo ni un objetivo de FPS sin medirlo.
