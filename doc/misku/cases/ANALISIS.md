# Caso 03: estabilidad del actualizador de Misku Terminal

Fecha de revisión: 2026-10-03 a 2026-10-04, America/Bogota. Revisión estática del WIP; preparación para una futura implementación.

**Dictamen:** el actualizador todavía no está listo para distribuirse. El bloqueo de compilación sigue presente en las fuentes copiadas. Antes de firmar o publicar conviene resolver también la recuperación de `Pending`, la coordinación entre procesos, la limpieza del firmador y la separación entre construir, firmar y publicar. La intención de preparar la actualización sin forzar el cierre de terminales está reflejada en el código, pero su funcionamiento y la conservación de datos requieren una prueba de despliegue.

## Alcance y evidencia

- Worktree revisado: `C:\Users\johan\.codex\worktrees\misku-updater-stability\misku-terminal`.
- `HEAD` y la referencia local `origin/feat/misku-unified-experience`: `4aa01e3f8bcb9845e3216660dbfb595e0c6f5176`. La referencia local `origin/main` es `dd494ac79a82a04e1e7252a91c8939a3c3039908`. No se hizo fetch ni se verificó el estado remoto actual.
- Los **22/22 archivos WIP** coinciden por SHA256 con `.misku-validation/case-evidence/pending-files.json`. Son 14 archivos rastreados modificados y 8 archivos nuevos. `git diff` por sí solo no incluye estos últimos.
- Evidencia histórica: `.misku-validation/case-evidence/build-updater-3.log`, SHA256 `b764da4903ce3dec490ee0fef1126189333ef5ba9fa4e8f99827b3238c99d8b0`. Contiene rutas de `C:\dev\misku-terminal`; es evidencia archivada, no una compilación de este worktree.
- El caso suministrado refiere CI `34172120891` y una instalación `0.1.2.0`. No se consultaron de nuevo; no acreditan el WIP ni una actualización exitosa.
- Comprobaciones nuevas: análisis sintáctico de los **11 scripts PowerShell** de `tools/misku` sin errores; lectura XML correcta de los **7 archivos** revisados —manifiesto Release, ambos XAML, ambos recursos y ambos proyectos—. Esto no valida enlaces XAML, APIs, firma ni comportamiento.
- `git diff --check` inicialmente señala CR como whitespace. La comprobación con `core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol`, pasada solo al comando, termina con código 0. No se cambió la configuración Git ni se normalizaron los archivos WIP.
- Se leyeron el caso, servicio, modelo, integración UI, pruebas, proyectos, recursos, manifiesto, cuatro scripts de firma/publicación, workflow y herramientas existentes de build/test/deploy. No se compiló, ejecutó ninguna suite, inició Misku, ejecutó ningún script operativo, inspeccionó claves privadas, configuró secretos o certificados, publicó ni modificó código.

Las ubicaciones siguientes son relativas al worktree. Los rangos identifican evidencia, no cambios aplicados. «Confirmado» significa observable en las fuentes o en el log; «riesgo» identifica un resultado que falta reproducir.

## Prioridades

| ID | Prioridad | Hallazgo | Evidencia disponible |
| --- | --- | --- | --- |
| F01 | P0 | Firma del handler incompatible con `InfoBar.CloseButtonClick` | Error C2664 archivado y firma todavía presente |
| F02 | P1 | `Pending` puede bloquear indefinidamente comprobación, instalación y preferencias | Recuperación desde un string; falta reconciliación con Windows |
| F03 | P1 | El lock serializa operaciones, pero no actualiza estados entre procesos | Singleton, snapshots y preferencias por proceso |
| F04 | P1 | El firmador puede borrar una identidad privada preexistente | Importación y `Remove-Item -DeleteKey` sin comprobar propiedad |
| F05 | P1 | `build-only` firma; push a main publica stable; falta frontera declarada para firma | Condiciones del workflow y acceso a secretos |
| F06 | P2 | Caché obsoleta tras 404 y pérdida de oferta al cambiar de canal | Caché persistida y caminos de retorno por throttling |
| F07 | P2 | Recuperación incompleta de descargas abandonadas y operaciones lentas | Limpieza solo al terminar; timeouts parciales |
| F08 | P2 | Reintentos y promoción de canal tienen límites concretos | Comparación de assets y dos reemplazos de discovery separados |
| F09 | P1 antes de release | Falta validación independiente completa del artefacto final y su confianza | CMS, identidad, SHA y validadores distribuidos entre scripts |

P0 bloquea la primera compilación. P1 debe cerrarse antes de una prueba firmada sobre datos importantes o de habilitar publicación. P2 debe resolverse o quedar cubierto por una política explícita y pruebas antes de abrir el canal.

## F01. El error raíz es C2664, no MSB4181

**Confirmado.** `TerminalPage.xaml:132` conecta `InfoBar.CloseButtonClick` con `_MiskuUpdateDismiss`; `TerminalPage.xaml:140` reutiliza ese handler para `Button.Click`. La declaración en `TerminalPage.h:425` y la definición en `TerminalPage.cpp:982` reciben `RoutedEventArgs` como segundo parámetro.

El log, líneas 76–101, muestra que el código generado pasa `Windows::Foundation::IInspectable` y falla en `TerminalPage.xaml.g.hpp(285,50)`. `MSB4181`, línea 120, aparece después al propagarse el fallo desde `TerminalAppLib`. La [documentación de WinUI 2.8](https://learn.microsoft.com/en-us/windows/winui/api/microsoft.ui.xaml.controls.infobar.closebuttonclick?view=winui-2.8) confirma el segundo argumento `IInspectable` de `CloseButtonClick`.

**Propuesta mínima:** separar el handler tipado del cierre y el de «Después», con ambos delegando en `Service::Dismiss`, o usar una firma común compatible con ambos eventos. Cambiar las fuentes, nunca el archivo generado. No hace falta modificar la barra ni los casos 01/02.

**Aceptación:** build Release x64 completo del WIP, XAML generado sin C2664 y paquete producido; después comprobar ambos mecanismos de dismiss. Un parser XML correcto no sustituye esta comprobación.

## F02. `Ready` no está reconciliado con el despliegue

**Confirmado en código.** `MiskuUpdateService.cpp:124–130` reconstruye `Ready` cuando el feed cacheado es más nuevo y su versión coincide con `MiskuUpdates.Pending`. El único registro de este marcador está en `:321`; no hay un camino para retirarlo ni una consulta del estado real de despliegue. `Check:200`, `Install:256` y `Preferences:357` retornan cuando el estado es `Ready`. El diálogo deshabilita los controles correspondientes en `AboutDialog.cpp:89–100`.

**Riesgo pendiente de reproducir:** si el registro diferido no termina, se cancela externamente o se revierte y la versión actual sigue siendo antigua, el siguiente proceso puede mostrar una actualización preparada indefinidamente. Cuando la versión nueva sí arranca, la comparación contra `_installedVersion` evita recuperar esa oferta; por ello no se afirma que toda actualización exitosa quede bloqueada.

Hay otra ventana de inconsistencia: después de que `AddPackageByUriAsync` termina, guardar la preferencia puede fallar (`:319–327`). La UI pasaría a `Error` aunque Windows pudiera haber preparado el paquete. El texto `Resources/es-ES/Resources.resw:1117` promete que la versión actual sigue intacta sin comprobar el resultado posterior al despliegue.

**Propuesta mínima:** persistir identidad/versión/canal y evidencia de la solicitud como pista de recuperación; reconciliarla al arrancar y antes de rechazar una acción. Distinguir «registrado», «preparado pendiente de activación», «ausente/fallido» y «resultado todavía desconocido». Retirar marcadores obsoletos cuando corresponda. Una entrada cacheada o la mera presencia de una versión staged no demuestra que exista una solicitud diferida válida para este usuario.

**Aceptación:** arranque tras éxito, cancelación, fallo, marcador falso, fallo de escritura después del despliegue y cierre inesperado. Cada caso recupera un estado accionable y concordante con Windows; el usuario puede buscar o reintentar cuando ya no existe una actualización pendiente real.

## F03. Operaciones serializadas y vistas obsoletas entre procesos

**Lo que ya protege el WIP:** `_mutex` y `_busy` coordinan las ventanas dentro de un proceso. `AcquireUpdateLock`, `MiskuUpdateService.cpp:68–75`, abre un archivo exclusivo en LocalCache y su handle permanece durante comprobación o instalación (`:205`, `:264`). Esto reduce operaciones simultáneas entre procesos del mismo usuario y la misma familia de paquete. Windows libera el handle si muere el proceso.

**Confirmado en código:** la versión instalada y el snapshot se capturan por proceso (`:105–130`). Después de conseguir el lock no se vuelve a leer `Pending`, las preferencias ni el despliegue. `Changed` es un evento dentro del proceso; no hay suscripción a cambios de AppData ni otro mecanismo de notificación entre procesos. `Preferences` y `Dismiss` escriben fuera del lock de archivo (`:334–369`).

**Riesgo reproducible propuesto:** abrir A y B sobre la misma versión, preparar desde A, esperar a que libere el lock e instalar desde B con su oferta antigua. El lock evita el solapamiento pero B puede repetir la preparación o preparar otra versión/canal desde su snapshot. A puede quedar en `Ready(v1)` mientras el marcador compartido pasa a `v2`. No se ha observado el resultado real del servicio de despliegue ni se afirma que esta carrera mate shells.

La comprobación que pierde el lock retorna sin notificar (`:206`); tampoco distingue contención de un error de acceso. La instalación perdedora muestra `ERROR_BUSY` (`:265`) sin renovar la vista del proceso.

**Propuesta mínima:** adquirir el lock y después reconciliar estado y preferencias compartidas antes de decidir; asociar operaciones a una identidad/versión de solicitud. Sincronizar las vistas al cambiar el estado compartido o al recuperar foco. Tratar contención con espera/reintento acotado y distinguirla de otros errores. No extrapolar este lock a cuentas Windows distintas.

**Aceptación:** dos ventanas de un proceso, dos procesos independientes y una instancia elevada del mismo usuario; clicks simultáneos y secuenciales; cambio de canal mientras otro proceso opera; caída del propietario del lock. Una única preparación coherente y vistas que convergen, sin desbloquear una operación que Windows todavía esté ejecutando.

## F04. La limpieza del firmador no conserva necesariamente la clave local

**Defecto condicional confirmado.** En `Sign-MiskuPackage.ps1:24–28`, la ruta por variables de entorno importa un PFX en `Cert:\CurrentUser\My` y marca `$imported = $true`. En `:55` elimina el certificado y su clave con `-DeleteKey` sin haber comprobado si la misma identidad ya existía. Si se usa un PFX de la identidad persistente de ese usuario en su máquina habitual, la limpieza puede borrar la clave que `Initialize-MiskuSigning` necesita conservar. No se ejecutó ni se reprodujo una pérdida de clave.

La ruta `-CertificateThumbprint` evita esa limpieza. También son aciertos el PFX temporal aleatorio, el borrado en `finally`, el uso del almacén personal y la ausencia de una importación automática a Trusted Root. Esas medidas no resuelven la propiedad de la clave importada.

**Propuesta mínima:** firmar en un contexto aislado o registrar de antemano si existían certificado y clave; limpiar únicamente recursos creados exclusivamente por esa ejecución. Probar éxito y fallo sin usar la identidad real del usuario.

`Initialize-MiskuSigning.ps1:15–27` conserva el thumbprint elegido y se niega a generar una identidad distinta si falta la clave guardada. Su copia DPAPI (`:86–92`) sirve para ese usuario y máquina; no acredita recuperación ante pérdida de ese equipo. La actualización de los dos secretos es secuencial (`:93–95`): si falla la segunda llamada, puede quedar una pareja incompatible hasta un reintento. Se necesita un contrato verificable de recuperación y de versión de la identidad, sin registrar sus valores.

**Aceptación:** inventario de thumbprints y presencia de clave privada iguales antes/después cuando la identidad era preexistente; importación temporal nueva retirada al terminar; éxito y excepciones; reintento tras actualizar solo un secreto. No se presupone ningún certificado, backup ni secreto disponible actualmente.

## F05. `build-only` no es una fase sin firma y main publica stable

**Confirmado.** El paso de firma y el upload firmado usan únicamente `github.event_name != 'pull_request'` (`misku-ci-cd.yml:280–309`). Una ejecución manual `build-only` entra ahí y `Sign-MiskuPackage.ps1:21–22` exige ambos secretos. Aunque `release` sí se omite, no existe una construcción manual completa independiente de la identidad de firma.

El default de `workflow_dispatch` es `prerelease` (`:15`), mientras que un push selecciona `stable` (`:79–90`) y publica (`:324–326`). `doc/misku/ci-cd.md:8–19` describe todavía prereleases 0.2 y artefactos sin firma. Esa documentación contradice la conducta del WIP.

**Fronteras presentes:** los PR no entran a los pasos de firma ni al job release; el token es read-only por defecto y `contents: write` queda en release. No hay `pull_request_target`. No se afirma una filtración de secretos desde PR.

**Riesgo de seguridad que falta contrastar con configuración remota:** los pasos firmantes ejecutan scripts del checkout en cualquier `workflow_dispatch` permitido. El YAML no restringe esa ruta a una ref confiable ni declara un environment protegido para la clave. Si un actor puede ejecutar una rama con código alterado y acceder a secretos de repositorio, la separación de eventos no basta. No se consultaron permisos, reglas, environments ni disponibilidad de secretos. Los [environments de GitHub](https://docs.github.com/en/actions/reference/workflows-and-actions/deployments-and-environments) permiten limitar ramas y el momento en que un job obtiene sus secretos; la disponibilidad concreta depende de la configuración del repositorio.

**Propuesta mínima:** construir y ejecutar pruebas sin secretos para PR y `build-only`; firmar después en un job separado, sobre un artefacto identificado y desde refs autorizadas; publicar mediante una fase explícita. Durante estabilización, push a main debe poder validar sin adelantar automáticamente el canal. Actualizar la documentación al contrato elegido. No configurar nada remoto en esta revisión.

**Aceptación:** matriz PR/build-only/firma/preview/stable con resultado y permisos esperados; PR y build-only pasan sin PFX ni password; ninguna ejecución de código no autorizado obtiene la clave; una ejecución de construcción no crea ni cambia releases. Registrar commit, hash de artefacto y procedencia entre jobs.

## F06. Caché, cambios de canal y preferencias

**Defectos deducibles del código:**

1. En un 404, `Check:218–223` limpia la oferta en memoria pero conserva el feed persistido. Al volver a arrancar, `:124–130` puede recuperar la oferta retirada; el `LastCheck` recién escrito impide refrescarla durante 24 horas. Si se trata de una retirada por seguridad o de un asset eliminado, seguir ofreciéndola resulta especialmente problemático.
2. Al cambiar canal, `Preferences:361–366` descarta la oferta y deja `Idle` sin recuperar el feed cacheado del canal elegido. `_UpdatePreferences`, `AboutDialog.cpp:123–129`, llama a una comprobación automática. Si ese canal tiene `LastCheck` reciente, `Check:208` retorna y la oferta conocida desaparece hasta una búsqueda manual o la siguiente comprobación.

**Aciertos:** Feed/LastCheck/Dismissed usan claves distintas por canal, `ParseFeed` exige el booleano `preview` correcto y `Install` solo se llama por un botón; buscar automáticamente no equivale a autorizar la instalación. Desactivar búsquedas no debería confundirse con cancelar una preparación previamente aceptada.

El texto «Incluir versiones preliminares» (`Resources/es-ES/Resources.resw:1090`) sugiere incluir preview junto a stable, pero el servicio consulta exclusivamente uno de los feeds. Ambos canales comparten identidad, nombre de paquete y tags `misku-vX.Y.Z`; no son dos instalaciones independientes. `ForceUpdateFromAnyVersion(false)` y el filtro de versión impiden un downgrade automático al volver de preview a un stable más antiguo.

**Propuesta mínima:** retirar o invalidar la caché correspondiente al 404; recuperar la caché válida del canal seleccionado antes del throttling. Decidir si preview significa seleccionar ese canal o escoger la mayor versión entre stable y preview, y ajustar la etiqueta. Mantener preferencias de comprobación editables cuando sea posible, con semántica clara para una instalación ya consentida.

**Aceptación:** retirar feed y reiniciar; cambiar ida/vuelta con ambas cachés recientes; automatic off con y sin caché; dismiss por canal/versión y búsqueda manual; stable inferior después de preview. No ofrecer una versión retirada por la respuesta actual ni simular una reversión automática.

## F07. Timeouts, archivos huérfanos y recuperación

El WIP limita el feed a 16 KiB, el paquete a 512 MiB y las lecturas HTTP del paquete a 30 segundos. Comprueba un plazo de 10 minutos dentro del bucle (`MiskuUpdateService.cpp:274–309`). Son defensas útiles; el límite de bytes sigue activo cuando no hay `Content-Length`.

**Omisiones confirmadas:** el plazo total no cubre todas las esperas. `ReadAsInputStreamAsync`, escritura, flush, apertura de archivos y `AddPackageByUriAsync` no tienen un timeout asociado (`:226`, `:277–279`, `:296`, `:305`, `:319`). Mientras una espera no termina, `_busy` y el lock pueden seguir ocupados; una retención real necesita reproducirse.

La descarga usa nombres únicos (`:276`) y se borra al terminar (`:328–330`). Una caída del proceso o un fallo del borrado deja bytes huérfanos; no hay barrido posterior ni presupuesto global de caché. El límite individual de 512 MiB no limita la suma de reintentos abandonados.

**Propuesta mínima:** definir deadline y observación de cada fase; conservar el handle de la operación de despliegue y reconciliar su resultado cuando sea incierto. No liberar el lock y lanzar otra instalación solo porque venció un temporizador si la anterior sigue viva. Limpiar descargas propias abandonadas al adquirir el lock, con identificación y límites de edad/volumen; conservar las que una operación válida todavía necesite. Registrar HRESULT, fase y resultado de despliegue, incluidos ActivityId/ErrorText cuando existan, sin contenido de shells ni secretos.

**Aceptación:** servidor que no envía cuerpo, lecturas lentas, escritura/flush fallidos, disco lleno, caída tras crear archivo y durante despliegue, limpieza fallida y reinicio. Se recupera el control, se limita el espacio acumulado y no se da una preparación por fallida o exitosa sin contrastar Windows.

## F08. Publicación, reintentos y canales

**Protecciones presentes en `Publish-MiskuUpdate.ps1`:** hash local del MSIX (`:17`), rechazo de downgrade del feed y de misma versión con otro hash (`:29–30`), draft inicial, comparación de assets cuando la release ya es pública (`:54–66`), ausencia de clobber del paquete público y discovery al final. El workflow serializa su job de publicación con `misku-update-publication` (`:328–330`). Estas decisiones reducen riesgos; no prueban la ejecución del flujo.

**Limitación concreta:** `New-MiskuUpdateArtifacts.ps1:35,73` incorpora el canal en el `Uri` de `MiskuTerminal.appinstaller`. Ese archivo se incluye entre los assets inmutables de una release (`Publish:53`). Regenerar la misma versión para promover preview a stable cambia ese asset y la comparación `:60–64` rechaza el reintento aunque el MSIX sea idéntico. Además, reutilizar una release pública no modifica su condición prerelease/latest: la edición está solo en la rama de draft (`:67–72`). La promoción de una versión existente no tiene un contrato completo.

**Riesgos y límites pendientes:**

- El reemplazo del `.appinstaller` y el de `update.json` son dos operaciones (`:79–82`). Una interrupción deja ambos discovery apuntando a versiones distintas; clobber también puede dejar un asset temporalmente ausente. No existe transacción ni recuperación automática demostrada. La aplicación solo consulta JSON, mientras una instalación inicial puede usar App Installer.
- Comparar `targetCommitish` con `$Commit` (`:43–47`) no acredita el commit resuelto del tag existente. Se debe verificar el objeto del tag antes de reutilizar una versión.
- Fallos de `gh release view` se tratan como ausencia (`:23–24`, `:43–44`), incluyendo fallos de autenticación o red. Las creaciones suelen detenerse si falla después, pero conviene diferenciar 404 de resultados desconocidos antes de hacer cambios.
- La exclusión de CI no coordina una ejecución manual del script desde otra máquina. Falta contrastar el estado del canal inmediatamente antes de avanzar discovery o exigir un único publicador.
- No se descargan de nuevo los assets recién publicados antes de publicar el feed. Debe verificarse que las URLs de paquete/dependencia entregan los bytes esperados y que un reintento después de cada frontera funciona.

**Propuesta mínima:** documentar promoción por nueva versión, o separar assets inmutables del instalador de canal y diseñar promoción de metadata. Guardar un inventario exacto de assets; verificar tag y artefactos remotos; distinguir errores de consulta; validar y reconciliar el par de discovery al reintentar. No prometer atomicidad entre dos assets separados.

**Aceptación:** fallo después de crear draft, después de subir cada asset, después de publicar la release y entre ambos discovery; versión igual/menor/mayor; tag ajeno; asset alterado; dos publicadores. El paquete público nunca se reemplaza con otros bytes y un reintento no degrada el canal. Probar estas decisiones primero con un cliente `gh` simulado, sin publicación real.

## F09. SHA256, identidad, firma y dependencias son verificaciones distintas

**Controles existentes:** `MiskuUpdateModel.h:22–39,64–72` exige versión canónica, revisión cero, hash hex minúsculo y una URL inicial exacta del repositorio/release/asset. `ParseFeed`, `MiskuUpdateService.cpp:174–189`, valida schema, identidad, publisher, arquitectura y canal. Se calcula el hash de los bytes descargados y se compara (`:280–309`); `ValidatePackage:78–101` comprueba identidad/versión/arquitectura del manifiesto real.

El lector [IAppxFactory::CreatePackageReader](https://learn.microsoft.com/en-us/windows/win32/api/appxpackaging/nf-appxpackaging-iappxfactory-createpackagereader) no valida la firma digital. La instalación usa opciones sin `AllowUnsigned` habilitado y depende del servicio de despliegue de Windows para aceptar el paquete. La ausencia de una comprobación de firma en `ValidatePackage` **no demuestra** que Windows acepte paquetes sin firma.

`Sign-MiskuPackage.ps1:38–50` valida el CMS de `AppxSignature.p7x` y compara el firmante contra el certificado seleccionado. [SignedCms.CheckSignature(true)](https://learn.microsoft.com/en-us/dotnet/api/system.security.cryptography.pkcs.signedcms.checksignature) comprueba firmas del mensaje sin validar los certificados. Este paso no verifica por sí solo toda la correspondencia entre el payload ZIP final y los hashes firmados ni su confianza en un cliente Windows. La firma inicial con SignTool y la comprobación CMS son evidencia parcial, no prueba de instalación.

`New-MiskuUpdateArtifacts` valida identidad y versión y genera SHA, pero no exige firma ni limita el tamaño al máximo del cliente. `Publish` comprueba hash/canal, sin volver a validar schema, URI, identidad real, firma o concordancia del certificado público. El workflow encadena estos scripts en el orden correcto; ejecutarlos individualmente exige un contrato más fuerte de entrada. La validación temprana de versión del workflow admite ceros iniciales (`:69–76`), mientras el generador los rechaza: conviene fallar antes de una compilación costosa.

**Propuesta mínima:** validar el artefacto final con un verificador de paquete que compruebe la integridad de su contenido y firmante; comprobar la confianza requerida en un entorno aislado de prueba. Derivar feed/installer/checksums desde ese mismo MSIX y volver a comprobarlos antes de publicar. Eliminar ambigüedad sobre la identidad esperada y su continuidad sin rotación accidental. La [documentación de firma programática de MSIX](https://learn.microsoft.com/en-us/windows/win32/appxpkg/how-to-programmatically-sign-a-package) describe verificación mediante WinVerifyTrust; elegir una validación adecuada al paquete y a la política de confianza de pruebas.

El generador comprueba una dependencia WinUI cuando el manifiesto construido la declara (`New-MiskuUpdateArtifacts:43–65`). El servicio no configura `DependencyPackageUris` en su instalación directa (`MiskuUpdateService:313–319`). No se ha inspeccionado un paquete construido de este WIP; NuGet/MSBuild pueden añadir dependencias al manifiesto final. Hace falta probar la actualización cuando WinUI requerido ya existe y cuando necesita actualizarse. Una dependencia incluida en `.appinstaller` no se incorpora automáticamente al instalar directamente el MSIX local desde el servicio.

Las redirecciones se comprueban solo por el esquema final HTTPS (`:217`, `:272`). Falta una prueba de los redirects legítimos de assets GitHub y de destinos inesperados. Una eventual restricción de host debe contemplar los CDN reales; SHA256 y confianza del paquete siguen siendo barreras independientes.

**Aceptación:** rechazar hash distinto, paquete vacío/excesivo, identidad/arquitectura/versión incorrectas, paquete sin firma, payload alterado conservando el CMS y firmante no confiable; verificar continuidad del certificado y coincidencia de CER/MSIX/feed/installer. No afirmar que un hash válido autentica al editor.

## Máquina de estados y experiencia prevista

| Situación | Conducta actual | Criterio para estabilizar |
| --- | --- | --- |
| Portable o identidad distinta | `Unsupported` | Mantenerlo explícito; no instalar sobre una app distinta |
| Arranque MSIX | Caché y comprobación tras 5 s; timer cada 24 h | Arranque ágil, recuperación de estado y sin red si automatic está desactivado |
| Comprobación manual | Evita throttling; no puede salir de `Ready` | Recuperación manual disponible cuando el despliegue pendiente ya no existe |
| Descarga/validación/despliegue | Un único estado `Downloading`, hasta 95 % antes del despliegue | Distinguir fases para diagnosticar y evitar progreso aparentemente congelado |
| Paquete preparado | `Ready` y marcador string | Confirmar solicitud diferida; retirar marcador después de completar o fallar |
| Error | HRESULT interno, texto genérico según si hay oferta | Separar comprobación, descarga, validación y despliegue incierto; permitir reintento seguro |

`DeferRegistrationWhenPackagesAreInUse(true)` junto a ambos flags ForceShutdown false (`MiskuUpdateService.cpp:314–317`) es coherente con la conservación de trabajo activo. Según [Microsoft](https://learn.microsoft.com/en-us/uwp/api/windows.management.deployment.addpackageoptions.deferregistrationwhenpackagesareinuse), si el paquete está en uso se difiere el registro hasta la siguiente activación. Eso no acredita la supervivencia de procesos ConPTY ni de datos del usuario en esta implementación.

La API [AddPackageByUriAsync](https://learn.microsoft.com/en-us/uwp/api/windows.management.deployment.packagemanager.addpackagebyuriasync) está disponible desde Windows 10 2004, consistente con `MinVersion=10.0.19041.0` del manifiesto. Falta verificar el URI local construido desde `download.Path()`, los permisos de una instancia normal/elevada y el comportamiento en la versión mínima y las versiones de Windows objetivo. No se propone lanzar Misku para sortear la restricción GUI existente.

## Cobertura y plan de estabilización

Existen **13 métodos** en `MiskuUpdateTests.cpp`: versiones canónicas/incorrectas/orden, feed válido, canales, identidad, schema, URI, SHA, tamaño/sintaxis, throttling y avisos. `Test-Misku.ps1:72–73` los selecciona y `:60–62` exige selección no vacía y sin fallos/omisiones. Los proyectos incluyen los tres archivos nuevos del servicio y la nueva suite. No se ejecutó ninguno de esos métodos en esta revisión.

`TabTests::MiskuUpdateNoticeStates`, `TabTests.cpp:344–366`, comprueba renderizado de Available/Ready/Dismissed y selección/cantidad de tabs. Solo entra por `-UI`; el workflow actual no usa esa opción. No comprueba descarga, firma, preparación MSIX, concurrencia ni procesos shell. Los resultados antiguos de `doc/misku/validation.md` pertenecen a la integración baseline.

### Etapa 1: desbloquear y acreditar compilación

Aplicar únicamente la corrección de eventos F01 en una fase futura autorizada. Compilar una copia Release x64 desde este worktree. Conservar log/binlog, commit o diff exacto, resultado final y SHA del paquete. No arrastrar artefactos de una compilación anterior para afirmar éxito.

**Salida:** build completo correcto, fuentes generadas coherentes y paquete identificable. El trabajo de sesiones y barra de los otros casos queda fuera de este cambio.

### Etapa 2: cerrar estados y probar fallos sin despliegue real

Introducir dependencias sustituibles para reloj, feed/red, settings, lock, archivos y consulta/operación de despliegue. Implementar recuperación y coordinación F02/F03/F06/F07. Probar con dobles los casos de fallo descritos, y scripts con archivos/almacenes temporales y `gh` simulado. Resolver F04 antes de utilizar una identidad persistente.

**Salida:** suites actuales y nuevas pasan, incluida selección no vacía; estados coherentes tras cada excepción/cierre; ningún script borra una clave ajena o publica durante pruebas. No basta añadir pruebas que repitan únicamente las condiciones del parser.

### Etapa 3: producir un paquete firmado verificable

Tras autorización específica para trabajar con una identidad de pruebas o con la identidad de distribución elegida, firmar el paquete validado. No generar automáticamente una nueva identidad si falta la usada en instalaciones existentes. Validar contenido completo, firma, publisher, versión, arquitectura, dependencias, tamaño y concordancia de todos los metadatos. Acreditar un plan de recuperación del firmante.

**Salida:** MSIX/CER/checksums/feed/installer concordantes, inventario de assets y procedencia verificable. Nada se publica ni se instala todavía sobre los datos habituales del usuario.

### Etapa 4: CI `build-only` independiente

Separar el workflow según F05. Ejecutar en una fase futura PR y `build-only` sin secretos; verificar build, tests, payload y uploads. La firma y publicación deben requerir un modo explícito y una ref admitida. Revisar configuración real de permisos/protecciones antes de habilitarlas.

**Salida:** evidencia nueva de CI sobre el cambio exacto; ninguna dependencia de secretos para construir y ningún cambio de release o canal. Los scripts operativos no deben ejecutarse simplemente para comprobar su sintaxis.

### Etapa 5: actualización real con datos y rollback comprobados

Usar un entorno Windows aislado y autorizado para GUI/despliegue. Instalar baseline con la identidad elegida, crear datos de prueba y respaldar `config.misku`, settings, estado de sesiones/workspaces y archivos del usuario pertinentes. Registrar versión/familia/identidad y procesos/PID antes de preparar la siguiente versión.

| Prueba | Evidencia de aceptación |
| --- | --- |
| Preparar con dos ventanas, panes y shells con trabajo activo | Los mismos PID siguen vivos, salida y entrada siguen funcionando, ningún cierre forzado |
| Cerrar una ventana y después todas cuando el usuario termina | La preparación no obliga al cierre; la siguiente activación arranca la versión esperada |
| Verificar datos después de activar | Configuración y archivos conservados; sesiones/workspaces restaurables según su contrato; no prometer reanudar una shell terminada |
| Hash/identidad/firma/dependencia/red/disco inválidos | Versión previa utilizable, datos conservados, error diagnosticable y reintento recuperable |
| Caída y reinicio en cada frontera | Windows y el estado del servicio coinciden; no quedan `Ready` falso ni descargas acumulándose sin límite |
| Fallo de registro diferido | Paquete anterior y datos verificables; marcador reconciliado, ninguna falsa afirmación de éxito |
| Nueva versión instalada pero defectuosa | Procedimiento ensayado para recuperar funcionamiento y datos; no depender de bajar el feed ni habilitar ForceUpdateFromAnyVersion |

Rollback del despliegue, retirada de un feed y recuperación de datos son operaciones diferentes. No hay una rutina explícita de rollback en el servicio. `Deploy-MiskuDebug.ps1:25–31` solo invoca DeployAppRecipe y verifica su salida; no acredita actualización del usuario ni restauración. Para una versión ya registrada, documentar y probar recuperación en ese entorno aislado antes de decidir una estrategia, incluyendo restricciones de versión y posibles migraciones de datos. No desinstalar/reinstalar la instalación real como supuesto rollback seguro.

**Salida:** prueba de actualización y recuperación con evidencias de paquete/procesos/datos, sin eludir la política GUI. Si el entorno autorizado no está disponible, esta etapa permanece pendiente y el canal no se considera validado.

### Etapa 6: abrir canal después de la validación

Primero ensayar todas las fronteras de publicación mediante `gh` simulado. Después, y solo en una fase de publicación expresamente autorizada, verificar artefactos remotos y anunciar preview; promover stable según el contrato de versiones elegido. Publicar discovery al final con verificación y recuperación de ambos archivos. Esta revisión no autoriza ni ejecuta esa etapa.

## Verificaciones que siguen faltando

Build nuevo y ejecución de las suites del WIP; renderizado XAML autorizado; operaciones concurrentes reales; recuperación de Pending; identidad y confianza de la firma elegida; integridad completa del paquete final; dependencias del manifiesto construido; actualización diferida sin afectar trabajo activo; preservación y recuperación de datos; fallo/rollback de despliegue; CI build-only nuevo; configuración remota de permisos/secretos; reintentos y publicación reales. El informe prepara esas verificaciones y no las presenta como realizadas.
