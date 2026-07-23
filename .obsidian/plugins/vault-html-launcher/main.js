const { FileSystemAdapter, Notice, Platform, Plugin } = require("obsidian");
const { shell } = require("electron");
const path = require("path");
const { pathToFileURL } = require("url");

function splitTarget(rawTarget) {
  const match = rawTarget.match(/^([^?#]+)(.*)$/);
  if (!match) {
    return null;
  }

  let relativePath;
  try {
    relativePath = decodeURIComponent(match[1]);
  } catch {
    relativePath = match[1];
  }

  return {
    relativePath: relativePath.replaceAll("\\", "/"),
    suffix: match[2],
  };
}

function isRelativeHtmlTarget(target) {
  if (!target || /^(?:https?|file|obsidian|data):/i.test(target)) {
    return false;
  }

  return /\.html(?:[?#].*)?$/i.test(target);
}

function resolveVaultPath(sourcePath, relativePath) {
  if (/^[a-zA-Z]:\//.test(relativePath)) {
    return null;
  }

  const sourceDirectory = path.posix.dirname(sourcePath);
  const target = relativePath.startsWith("/")
    ? relativePath.slice(1)
    : path.posix.join(sourceDirectory, relativePath);
  const normalized = path.posix.normalize(target);

  if (
    normalized === ".." ||
    normalized.startsWith("../") ||
    path.posix.isAbsolute(normalized)
  ) {
    return null;
  }

  return normalized;
}

module.exports = class VaultHtmlLauncher extends Plugin {
  onload() {
    if (!Platform.isDesktopApp) {
      return;
    }

    this.registerDomEvent(
      document,
      "click",
      (event) => this.handleClick(event),
      true,
    );
  }

  handleClick(event) {
    if (event.defaultPrevented || event.button !== 0) {
      return;
    }

    const targetElement =
      event.target instanceof Element ? event.target : event.target?.parentElement;
    const anchor = targetElement?.closest("a");
    if (!anchor) {
      return;
    }

    const rawTarget =
      anchor.getAttribute("data-href") || anchor.getAttribute("href");
    if (!isRelativeHtmlTarget(rawTarget)) {
      return;
    }

    event.preventDefault();
    event.stopImmediatePropagation();
    void this.openHtml(rawTarget);
  }

  async openHtml(rawTarget) {
    const sourceFile = this.app.workspace.getActiveFile();
    if (!sourceFile) {
      new Notice("Open the note containing this HTML link first.");
      return;
    }

    const parsed = splitTarget(rawTarget);
    const vaultPath = parsed
      ? resolveVaultPath(sourceFile.path, parsed.relativePath)
      : null;
    if (!vaultPath) {
      new Notice("This HTML link points outside the current vault.");
      return;
    }

    const vaultFile = this.app.vault.getAbstractFileByPath(vaultPath);
    if (!vaultFile || vaultFile.extension?.toLowerCase() !== "html") {
      new Notice(`HTML file not found: ${vaultPath}`);
      return;
    }

    const adapter = this.app.vault.adapter;
    if (!(adapter instanceof FileSystemAdapter)) {
      new Notice("The current vault does not expose a local file path.");
      return;
    }

    const basePath = path.resolve(adapter.getBasePath());
    const absolutePath = path.resolve(basePath, vaultPath);
    const pathWithinVault = path.relative(basePath, absolutePath);
    if (
      pathWithinVault === ".." ||
      pathWithinVault.startsWith(`..${path.sep}`) ||
      path.isAbsolute(pathWithinVault)
    ) {
      new Notice("Blocked an HTML link outside the current vault.");
      return;
    }

    try {
      const fileUrl = `${pathToFileURL(absolutePath).href}${parsed.suffix}`;
      await shell.openExternal(fileUrl);
    } catch (error) {
      console.error("Vault HTML Launcher: failed to open HTML file", error);
      new Notice(`Failed to open HTML file: ${vaultPath}`);
    }
  }
};
