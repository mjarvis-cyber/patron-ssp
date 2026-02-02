# bang

**bang** for Windows (based on a proof-of-concept by [Red Team Notes](https://www.ired.team/offensive-security/credential-access-and-credential-dumping/intercepting-logon-credentials-via-custom-security-support-provider-and-authentication-package)) is a Security Support Provider (SSP) that sends intercepted logon credentials from the Local Security Authority (LSA) to an HTTPS endpoint.

**bang** for Linux is a Pluggable Authentication Modules (PAM) module that sends intercepted logon credentials from the PAM authentication stack to an HTTPS endpoint.


## Building

### Windows
The recommended build environment is Visual Studio 2022. Building the solution `monstars.sln` will create an installer EXE within the `_export/bang` directory of the repo.

The SSP DLL is packaged as a resource into the installer EXE. On its own, the DLL is not required unless installing the SSP manually- if needed, it can be found in the `_build/` directory of the repo.

### Linux
Ensure the PAM development headers are installed in your build environment.

Use the Python script `scripts/build_pam_mod.py` to build an installer execuable. An unconfigured installer binary will be placed under the `_export/bang/pam/` directory of the repo.


## Django App

The `bang-swackhammer` Python package is a Django app used for handling `POST` requests sent by the SSP. The web interface can also be used to:
* view collected logon credentials
* list and/or revoke API tokens
* configure an installer EXE for deployment to a target

Usage of the web interface requires a user account on the Django webserver.

Before the webapp can be used to configure an installer, compiled installer binaries must be copied/moved from the `_export/` directory into `app/bang/installers/`.

### Packaging

For deployment, the `bang-swackhammer` package should be built as a Python wheel for easy installation in the webserver Django environment:
1. Build the un-configured installer binaries (see [Building](#building))
1. Use the `app_symlinks.py` helper script to generate installer symlinks within the local sourcetree (see [Helper Scripts](#app_symlinkspy))
1. Use the `build` Python package to build the wheel:
    ```bash
    $ python3 -m pip install build
    $ python3 -m build ./bang/app --wheel --outdir ./_export
    ```


## Helper Scripts

A few helper scripts are provided at `bang/scripts/`, for convenience when testing outside of a Django environment.

### app_symlinks.py
`app_symlinks.py` is a utility for creating symlinks to compiled installer binaries within the Django app package. It assumes compiled binaries are present in the `_export/` directory (see [Building](#building)) but will create symlinks even if no binaries exist.

Once symlinks have been created, the app sourcetree is usable as an editable python package. Symlinks are also followed when packaging the webapp for deployment (see [Django App: Packaging](#packaging)).

### configure.py
`configure.py` is a command-line interface for configuring an installer binary. It requires compiled binaries to be present in the `_export/` directory (see [Building](#building)).

### server.py
`server.py` is a simple HTTPS server capable of listening for credentials sent by the provider. The script requires a `bangsrv.pem` file in the same directory, containing a TLS server certificate and private key. Credentials are printed to the console, and not otherwise saved anywhere.


## Installation

### Windows
Run the configured installer EXE on the target. The installer handles:
* placing the SSP DLL in `C:\Windows\System32`
* transfering ownership of the SSP DLL to `TrustedInstaller`
* readjusting the created/modified/accessed timestamps for the SSP DLL and parent directory
* registering the SSP with LSA (no reboot required)

These operations require elevated privileges.

### Linux
Run the configured installer executable on the target. The installer handles:
* placing the PAM module in the correct directory for the distro
* adding an `auth` entry to an appropriate PAM stack for the distro
* readjusting the modified/accessed timestamps for the module and the auth stack
* setting the `nis_enabled` SELinux policy to enable outgoing network traffic from sshd

These operations require elevated privileges.

## Uninstallation

### Windows
Uninstalling the SSP is a manual process requiring access to the target machine:
1. Remove the SSP entry from the list of SSPs in the `Security Packages` value of the registry key `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Lsa`.
2. Reboot the target machine
3. Delete the SSP DLL from `C:\Windows\System32\`

### Linux
TODO