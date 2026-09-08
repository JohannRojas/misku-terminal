# Validación de la integración Misku

Fecha: 7 de septiembre de 2026. Rama local: `feat/misku-unified-experience`.

Integra las sesiones de [#3](https://github.com/JohannRojas/misku-terminal/pull/3) (`9a8b8fcf5529a4b274ef4b08eee596018e368ffe`) con la configuración y distribución de [#2](https://github.com/JohannRojas/misku-terminal/pull/2) (`f7cbd169b548a541b79245c3bb2a73ec9f91fa46`).

## Resultado

La compilación local Release x64 produce el MSIX `MiskuTerminal` 0.2.0.0 y un ZIP portable. En la ejecución final pasan las 233 pruebas, sin pruebas fallidas, bloqueadas, omitidas ni pendientes.

| Suite | Resultado | Cobertura principal |
| --- | ---: | --- |
| SettingsModel | 163/163 | Configuración, persistencia, validación y recuperación de `config.misku`, eliminación de overrides, resolución de todos los alias de atajos y atajos predeterminados |
| CommandlineTest | 43/43 | Comandos y sesiones; dimensiones restauradas a 96 y 144 DPI, con y sin barra lateral; migración de geometría anterior |
| TabTests, XAML nativo | 27/27 | Arranque, atajos, sesiones, movimientos y restauración; barra superior fijada; colores; cambio de pestaña, paneles, zoom y selección |

También se comprobaron la sintaxis PowerShell y los espacios del diff. El ejecutor falla si una selección ejecuta cero pruebas o deja pruebas omitidas.

Una ejecución previa dejó cuatro pruebas XAML bloqueadas al perder acceso a `AppXManifest.xml` durante el reinicio del contenedor TAEF. Se regeneró el manifiesto mediante la compilación del TestHost y se repitió la suite completa; las 27 pruebas pasaron. Se conserva este incidente de infraestructura para no confundirlo con una ejecución inicialmente limpia.

## Cambios comprobados

- La ventana guarda el tamaño completo del cliente y reconoce el formato anterior, evitando que se reduzca en cada restauración.
- La restauración espera a que terminen todas las acciones de arranque antes de descartar los índices de sesión y seleccionar la sesión guardada.
- La barra superior conserva su activación por eventos; al aparecer sobre el contenido no cambia el espacio reservado. Fijarla reserva su altura.
- La barra lateral conserva sus controles. Los recursos de color de cada pestaña se reutilizan cuando sus colores evaluados no cambian; los cambios de color y selección siguen aplicándose.
- `Ctrl+W` queda disponible para la shell; `Ctrl+Shift+W` mantiene el cierre de panel. Los atajos de pestañas, sesiones y barra lateral siguen siendo configurables.
- Se corrige `close_tab`, que apuntaba a un identificador de comando inexistente. Solo al configurarlo se crea su acción; las pruebas resuelven todos los alias a sus acciones reales y comprueban la dirección de las divisiones.
- `config.misku` admite recarga y eliminación, conserva la última versión válida ante errores y muestra un aviso no modal. Las pruebas del modelo cubren el parser y la aplicación de estos estados; no equivalen a una prueba interactiva del watcher en el ejecutable de escritorio.
- Se corrige la generación XAML para C++/WinRT puro con el SDK 26100. El contenedor de pruebas incluye los recursos, bibliotecas e iconos necesarios para probar los perfiles predeterminados.

## Medición de navegación

Prueba nativa Release, dos sesiones y 2, 10 o 50 pestañas. Se miden 30 cambios inmediatamente después de crear las pestañas (`after_open`) y otros 30 sin abrir más (`repeat`). Se cronometra `_SwitchToSession` más `UpdateLayout`, dentro del hilo de interfaz. El perfil de carga usa un historial mínimo para aislar esta interacción.

| Pestañas | Fase | Mediana | P95 | Máximo |
| ---: | --- | ---: | ---: | ---: |
| 2 | Tras abrir | 4.334 ms | 5.656 ms | 64.875 ms |
| 2 | Repetición | 4.451 ms | 5.447 ms | 5.452 ms |
| 10 | Tras abrir | 6.004 ms | 8.483 ms | 99.572 ms |
| 10 | Repetición | 7.211 ms | 9.640 ms | 10.326 ms |
| 50 | Tras abrir | 13.503 ms | 106.739 ms | 154.974 ms |
| 50 | Repetición | 18.616 ms | 33.190 ms | 37.247 ms |

Antes de reutilizar los recursos de color, la mediana con 50 pestañas era 168.187 ms y el P95 242.908 ms, usando la primera tanda del mismo escenario. Es una comparación diagnóstica en esta máquina, sin controlar toda la carga del sistema.

La tabla corresponde a la última ejecución completa. Las mediciones varían: en otra ejecución la repetición con 50 pestañas dio una mediana de 14.496 ms y P95 de 16.013 ms. El mayor pico observado entre las ejecuciones fue 273.756 ms tras crear 50 pestañas. La navegación mejora considerablemente, pero estos resultados no certifican ausencia de tirones durante la carga ni una frecuencia de imagen estable.

## Compilación y artefactos

- Visual Studio 2022, MSBuild 17.14.40, MSVC v143 14.44, Windows SDK 10.0.26100.0.
- Compilación local incremental con regeneración de XAML; la compilación desde cero en CI queda pendiente de publicar la rama.
- MSIX: `src/cascadia/CascadiaPackage/AppPackages/CascadiaPackage_0.2.0.0_x64_Test/CascadiaPackage_0.2.0.0_x64.msix`.
- SHA-256 del MSIX: `f5e0ccce2394fdab6d117f2398c2fa2f7eff4f8e79edd2c4fa3e4db0ccb86b05`.
- Portable: `artifacts/MiskuTerminal-0.2.0-x64-portable.zip`; checksum en el archivo `.zip.sha256` adyacente.
- Se verificó que el MSIX y el ZIP contienen los mismos `misku.exe`, `TerminalApp.dll` y `Microsoft.Terminal.Settings.Model.dll` que la compilación final. El ZIP incluye WinUI, el PRI combinado y `.portable`, sin PDB ni ajustes personales.
- El artefacto local no está firmado y no se instaló sobre la versión del usuario.

## Límites

La herramienta de control visual bloqueó la apertura del terminal de escritorio. La validación gráfica ejecutada corresponde al contenedor nativo XAML de pruebas. No se verificaron visualmente el ejecutable portable, el movimiento real de ventanas entre monitores ni la animación de la barra sobre una consola en uso.

No se midieron FPS, latencia de teclado a pantalla, consumo de memoria con historiales grandes ni rendimiento de salida sostenida de una shell. Las pruebas UAP usan conexiones limitadas por ese entorno y no sustituyen una sesión de uso real de ConPTY.

Las pruebas XAML usan el framework WinUI instalado; el portable incluye la versión fijada por el repositorio, cuyo archivo declara 2.8.2305.05001. La comprobación visual del portable sigue pendiente.

La compilación conserva avisos PRI263 sobre recursos traducidos sin entrada predeterminada y avisos del sistema de compilación XAML. Las suites ejecutadas pasan; no se afirma que todos los idiomas y combinaciones de ajustes estén cubiertos.

## Reproducir

```powershell
.\tools\misku\Build-MiskuDebug.ps1 -Platform x64 -Configuration Release -Branding Release
.\tools\misku\Test-Misku.ps1 -Platform x64 -Configuration Release -Branding Release -UI
```

Resultados detallados: `artifacts/tests/settings.log`, `commandline.log` y `sessions.log`. La opción `-UI` requiere un escritorio Windows y las dependencias del TestHost; CI ejecuta las suites de configuración y comandos.
