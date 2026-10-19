# Publish on GitHub

The project repository is [BulatTim1/desktop-light](https://github.com/BulatTim1/desktop-light).
For a new local checkout, clone that repository. To publish changes from an
existing checkout, review the diff, commit, and push:

```powershell
git status --short
git add .
git commit -m "Describe the change"
git push -u origin main
```

For a fresh source archive without Git metadata, first run `git init -b main`
and `git remote add origin https://github.com/BulatTim1/desktop-light.git`.
Review staged files before committing. `.gitignore` excludes runtime caches,
build output and logs. Never force-push over existing repository history.

The GitHub workflow builds on Windows with Visual Studio/CMake, runs tests,
prepares the private Windhawk runtime and extension, and uploads portable,
installer and matching source packages. It needs no repository secrets. The
build scripts download signed official Inno Setup and Windhawk toolchains into
ignored caches; neither toolchain is included in your Git repository.

After the build passes, publish version 1.2.1:

```powershell
git tag v1.2.1
git push origin v1.2.1
```

The tag workflow creates a GitHub Release containing the packages. For later
versions, update CMake's project version, app version metadata, installer default
and diagnostic version before tagging.

Builds are unsigned until you configure your own code-signing certificate. No
signing key or local diagnostic upload is included. Keep GPL attribution for the
Windhawk integration and its matching source archive. Do not distribute cached
installers, local settings, symbol caches or runtime logs. The packaging script
includes only a freshly prepared minimal runtime and an explicit source allowlist.
