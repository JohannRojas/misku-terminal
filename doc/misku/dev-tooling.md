# Misku Terminal development tooling

This project is wired for native Windows 11 development on top of Windows Terminal.

## Installed/local tools

Run this to inspect the local toolchain:

```powershell
.\tools\misku\Get-MiskuToolchain.ps1
```

Expected core tools:

- Visual Studio with the workloads declared in `.vsconfig`
- MSBuild
- DeployAppRecipe
- Windows SDK `inspect.exe`
- PowerShell 7
- Git
- ripgrep
- Windows Performance Recorder

## Common commands

Build the Debug x64 package:

```powershell
.\tools\misku\Build-MiskuDebug.ps1
```

Deploy the last Debug x64 package:

```powershell
.\tools\misku\Deploy-MiskuDebug.ps1
```

Deploy and launch:

```powershell
.\tools\misku\Deploy-MiskuDebug.ps1 -Launch
```

Launch the installed app:

```powershell
.\tools\misku\Launch-Misku.ps1
```

Restart the installed app:

```powershell
.\tools\misku\Launch-Misku.ps1 -Restart
```

Open the Windows SDK UI Automation inspector:

```powershell
.\tools\misku\Open-MiskuInspect.ps1
```

Run the settings and command-line regression tests after a Release build:

```powershell
.\tools\misku\Test-Misku.ps1 -Configuration Release -Branding Release
```

Capture a short startup performance trace:

```powershell
.\tools\misku\Start-MiskuWprTrace.ps1
```

## CI/CD

Pull requests and pushes to `main` are built by `.github/workflows/misku-ci-cd.yml`.
Successful pushes publish incremental portable prereleases; stable releases can be started manually.

See [ci-cd.md](ci-cd.md) for versioning, artifacts, permissions, and repository policy.

## Recommended loop

1. Edit the C++/XAML/settings code.
2. Run `.\tools\misku\Build-MiskuDebug.ps1`.
3. Run `.\tools\misku\Deploy-MiskuDebug.ps1 -Launch`.
4. Run `.\tools\misku\Test-Misku.ps1 -Configuration Debug -Branding Dev -UI` for native XAML regression tests in the isolated TestHost app.
5. Use `inspect.exe` for UI tree/debugging when an interaction is not visible to UIA.
6. Use `Start-MiskuWprTrace.ps1` when startup, output, resizing or tab operations feel slow.
