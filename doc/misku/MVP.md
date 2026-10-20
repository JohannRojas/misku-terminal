# Misku Terminal MVP

Misku Terminal is a Windows 11-first fork of Windows Terminal, pinned to upstream tag `v1.24.11321.0` and branded as a native terminal MVP.

## Build prerequisites

Install the Windows Terminal build prerequisites before building:

- Visual Studio with the workloads declared in `.vsconfig`
- Windows 11 SDK / Windows App SDK components required by Windows Terminal
- PowerShell 7 (`pwsh`) for build scripts
- Developer Mode enabled for local package registration

The main app target is now `misku.exe` from `src/cascadia/WindowsTerminal/WindowsTerminal.vcxproj`. The default package alias is also `misku.exe`.

## MVP behavior

- Defaults prefer generated PowerShell 7 profiles when present, then fall back to Windows PowerShell.
- WSL profiles are inherited from Windows Terminal's dynamic profile generator.
- Defaults use the `Misku Dark` theme, Cascadia Mono, 9001 scrollback lines, and an opaque background without acrylic or Mica.
- `Ctrl+T` opens a tab, `Ctrl+Shift+T` creates a session, and `Ctrl+B` toggles the session sidebar. These shortcuts are configurable. `Ctrl+W` remains available to the shell; `Ctrl+Shift+W` closes a pane.
- The top bar appears over the content without resizing the terminal on hover. Its pin button keeps it visible and reserves its space.
- Unpackaged settings are stored under `%LOCALAPPDATA%\MiskuTerminal`.
- Text/package identity is renamed for the MVP; regenerating Misku-specific PNG/icon assets requires a WSL distro with Inkscape and ImageMagick for `res/terminal/Generate-TerminalAssets.ps1`.

## config.misku

Misku reads the first available config file from:

1. The `MISKU_CONFIG` environment variable or `--config <path>` on the first process launch.
2. `%USERPROFILE%\.config\misku\config.misku`.
3. `%LOCALAPPDATA%\MiskuTerminal\config.misku`.

Supported keys are `font-family`, `font-size`, `theme`, `background`, `foreground`, `opacity`, `mica`, `default-profile`, `working-directory`, `scrollback-lines`, `cursor-style`, and repeated `keybind` lines.

Changes, replacements and deletion of `config.misku` trigger a throttled reload. A `reload_config` keybinding also reloads it. Invalid files show a dismissible in-app warning with the line number, write the error to `%LOCALAPPDATA%\MiskuTerminal\config.misku.error.log`, and keep the last valid in-process overlay. Deleting the file removes its overrides. Overlays are not persisted into `settings.json` during automatic migration.

`--theme <name>` sets `MISKU_THEME` before settings load and can be used as a launch-time theme override.
