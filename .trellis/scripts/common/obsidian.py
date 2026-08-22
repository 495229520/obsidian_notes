"""
Shared helpers for Trellis' local Obsidian integration scripts.
"""

from __future__ import annotations

import json
import os
import tempfile
from dataclasses import dataclass
from pathlib import Path


HOOKS_LOCAL_FILE = "hooks.local.json"


class ObsidianConfigError(RuntimeError):
    """Raised when local Obsidian integration config is missing or invalid."""


@dataclass(frozen=True)
class ObsidianConfig:
    """Validated local Obsidian integration settings."""

    vault_path: Path
    task_dashboard_dir: str
    language: str


def load_hooks_local_config(repo_root: Path) -> dict:
    """Load ``.trellis/hooks.local.json`` from the repository root."""
    config_path = repo_root / ".trellis" / HOOKS_LOCAL_FILE
    try:
        return json.loads(config_path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return {}
    except (OSError, json.JSONDecodeError) as exc:
        raise ObsidianConfigError(
            f"Failed to read local hook config at {config_path}: {exc}"
        ) from exc


def ensure_within(root: Path, candidate: Path) -> Path:
    """Return the resolved candidate path after enforcing containment."""
    resolved_root = root.resolve()
    resolved_candidate = candidate.resolve()
    try:
        resolved_candidate.relative_to(resolved_root)
    except ValueError as exc:
        raise ObsidianConfigError(
            f"Path escapes configured vault: {resolved_candidate}"
        ) from exc
    return resolved_candidate


def resolve_obsidian_config(
    repo_root: Path,
    *,
    config_path: Path | None = None,
    vault_path: str | None = None,
    task_dashboard_dir: str | None = None,
    language: str | None = None,
) -> ObsidianConfig:
    """Resolve and validate the local Obsidian config for this machine."""
    raw: dict = {}
    if config_path is not None:
        try:
            raw = json.loads(config_path.read_text(encoding="utf-8"))
        except FileNotFoundError as exc:
            raise ObsidianConfigError(
                f"Config file not found: {config_path}"
            ) from exc
        except (OSError, json.JSONDecodeError) as exc:
            raise ObsidianConfigError(
                f"Failed to read config file {config_path}: {exc}"
            ) from exc
    else:
        raw = load_hooks_local_config(repo_root)

    obsidian_raw = raw.get("obsidian")
    if not isinstance(obsidian_raw, dict):
        raise ObsidianConfigError(
            "Missing 'obsidian' object in .trellis/hooks.local.json"
        )

    vault_value = vault_path or obsidian_raw.get("vault_path")
    dashboard_value = task_dashboard_dir or obsidian_raw.get("task_dashboard_dir")
    language_value = language or obsidian_raw.get("language") or "zh"

    if not isinstance(vault_value, str) or not vault_value.strip():
        raise ObsidianConfigError("obsidian.vault_path must be a non-empty string")
    if not isinstance(dashboard_value, str) or not dashboard_value.strip():
        raise ObsidianConfigError(
            "obsidian.task_dashboard_dir must be a non-empty relative path"
        )
    if Path(dashboard_value).is_absolute():
        raise ObsidianConfigError("obsidian.task_dashboard_dir must be relative")

    vault_dir = Path(vault_value).expanduser().resolve()
    if not vault_dir.is_dir():
        raise ObsidianConfigError(f"Configured vault does not exist: {vault_dir}")
    if not (vault_dir / ".obsidian").is_dir():
        raise ObsidianConfigError(
            f"Configured vault is missing its .obsidian directory: {vault_dir}"
        )

    dashboard_path = ensure_within(vault_dir, vault_dir / dashboard_value)
    rel_dashboard = dashboard_path.relative_to(vault_dir).as_posix()
    return ObsidianConfig(
        vault_path=vault_dir,
        task_dashboard_dir=rel_dashboard,
        language=str(language_value or "zh"),
    )


def resolve_dashboard_output_path(
    config: ObsidianConfig,
    filename: str,
    *,
    output_path: str | None = None,
) -> Path:
    """Resolve a dashboard output file and keep it inside the dashboard dir."""
    base_dir = ensure_within(
        config.vault_path,
        config.vault_path / config.task_dashboard_dir,
    )
    if output_path:
        candidate = Path(output_path).expanduser()
        if not candidate.is_absolute():
            candidate = config.vault_path / candidate
    else:
        candidate = base_dir / filename
    # The exporter has no reason to write elsewhere in the vault. Checking the
    # narrower dashboard directory also rejects ``..`` and symlink escapes.
    return ensure_within(base_dir, candidate)


def atomic_write_text(path: Path, content: str) -> None:
    """Write UTF-8 text atomically, preserving the old file on failure."""
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(
        dir=str(path.parent),
        prefix=f".{path.name}.",
        suffix=".tmp",
        text=True,
    )
    try:
        try:
            handle = os.fdopen(fd, "w", encoding="utf-8")
        except OSError:
            os.close(fd)
            raise
        with handle:
            handle.write(content)
        os.replace(tmp, path)
    except OSError:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise
