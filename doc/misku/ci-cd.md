# Misku continuous integration and delivery

The `.github/workflows/misku-ci-cd.yml` workflow keeps pull-request validation and release publishing in one reproducible pipeline.

## Triggers

- Pull requests targeting `main` restore dependencies, compile the x64 Release package, validate the runtime payload, and upload a 30-day artifact.
- Every push to `main` performs the same checks and publishes an incremental prerelease named `misku-v1.0.<run number>`.
- Manual runs support `build-only`, `prerelease`, and `stable` modes. An optional version must use three numeric components, such as `1.2.3`.

A release is created only after the build and package validation job succeeds. Re-running the same workflow replaces assets on the existing tag instead of creating a duplicate release.

## Artifact format

The published asset is a portable x64 ZIP. It contains the Release runtime without PDB files, plus a short README and a separate SHA-256 checksum.

Extract the entire archive and run `misku.exe`. Keep the DLL, PRI, and resource files beside the executable.

Automated artifacts are intentionally unsigned until a trusted Windows code-signing certificate is configured. Windows may therefore display a SmartScreen warning.

## Local release build

Use the same build entry point as CI:

```powershell
.\tools\misku\Build-MiskuDebug.ps1 `
    -Platform x64 `
    -Configuration Release `
    -Branding Release
```

CI stamps the selected four-part MSIX version into its ephemeral manifest copy before building. Choose a public three-part version through the manual workflow input; the package revision remains `0`.

## Repository settings

The workflow requests read-only permissions by default. Only the release job requests `contents: write`, which is required to create GitHub releases.

After this workflow lands on `main`, configure the `main` branch ruleset to require the `Build and verify x64 Release` check before merging. Branch rules are repository policy and are not changed by the workflow itself.
