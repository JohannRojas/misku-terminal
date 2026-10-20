# Installing Misku Terminal

Misku Terminal is distributed as an x64 MSIX package. Installing the package registers the app in the Start menu, enables the `misku.exe` execution alias, and lets Windows manage updates and uninstallation.

## Development certificate releases

Community builds are signed with a self-signed development certificate. Windows requires trusting this certificate before installing the MSIX:

1. Download the `.msix` and `.cer` files from the same GitHub release.
2. Import the `.cer` file into **Local Computer > Trusted People**. This requires administrator approval.
3. Double-click the `.msix` file and choose **Install**.

Only trust the certificate when its SHA-256 fingerprint matches the value published in the release notes. Remove it from **Local Computer > Trusted People** if you no longer use Misku Terminal.

## Production distribution

A broadly distributed build should use Microsoft Store signing, Azure Artifact Signing, or another publicly trusted code-signing certificate. A self-signed certificate is intended only for development and tester distribution.
