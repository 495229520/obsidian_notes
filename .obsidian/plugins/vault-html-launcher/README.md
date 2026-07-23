# Vault HTML Launcher

This desktop-only Obsidian plugin opens relative `.html` links from notes in the
system browser. It resolves each link from the current vault location, so the
vault can be cloned into a different directory on another computer.

Supported link:

```markdown
[Open animation](<../chapter/example.html#scene=demo>)
```

After cloning the vault on a new computer:

1. Open the vault in Obsidian.
2. Open **Settings > Community plugins**.
3. Enable **Vault HTML Launcher**.

The plugin only opens HTML files located inside the current vault. Web URLs and
absolute `file://` links are left unchanged.
