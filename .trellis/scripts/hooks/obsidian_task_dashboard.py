#!/usr/bin/env python3
"""Export Trellis tasks into a deterministic, read-only Obsidian note."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path
from typing import Iterable

SCRIPT_DIR = Path(__file__).resolve().parent
SCRIPTS_DIR = SCRIPT_DIR.parent
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from common.obsidian import (  # noqa: E402
    ObsidianConfig,
    ObsidianConfigError,
    atomic_write_text,
    resolve_dashboard_output_path,
    resolve_obsidian_config,
)
from common.paths import DIR_ARCHIVE, DIR_TASKS, FILE_TASK_JSON, get_repo_root  # noqa: E402


GENERATED_BY = "trellis-obsidian-task-dashboard"
DETAIL_GENERATED_BY = "trellis-obsidian-task-detail"
OUTPUT_FILENAME = "_Trellis Task 数据源.md"
DETAIL_DIRNAME = "Trellis详情"
GENERATED_SOURCE_TAG = "#trellis/source/generated"

STATUS_LABELS = {
    "in_progress": "进行中",
    "planning": "规划中",
    "blocked": "受阻",
    "completed": "已完成",
}
STATUS_SECTION_ORDER = {
    "in_progress": 0,
    "planning": 1,
    "blocked": 2,
    "completed": 3,
}
PRIORITY_EMOJI = {
    "P0": "🔺",
    "P1": "⏫",
    "P2": "🔼",
    "P3": "🔽",
}
PRIORITY_ORDER = {level: index for index, level in enumerate(["P0", "P1", "P2", "P3"])}
HEADING_PATTERN = re.compile(r"^(#{1,6})\s+(.+?)\s*$")
CHECKBOX_PATTERN = re.compile(r"^\s*[-*+]\s+\[([ xX])\]\s+(.+?)\s*$")
FINISH_SECTION_HINTS = (
    "final",
    "finish",
    "handoff",
    "wrap",
    "commit",
    "archive",
    "release",
    "收尾",
    "交付",
    "提交",
    "归档",
    "发布",
)
MAX_INLINE_CHECKLIST_ITEMS = 8
DEFAULT_TASK_CHILD_INDENT = "\t"


@dataclass(frozen=True)
class ChecklistItem:
    """One read-only checklist item extracted from a Trellis artifact."""

    text: str
    completed: bool


@dataclass(frozen=True)
class ChecklistSection:
    """Checklist items grouped by their nearest Markdown heading."""

    title: str
    items: tuple[ChecklistItem, ...]

    @property
    def completed_count(self) -> int:
        return sum(item.completed for item in self.items)

    @property
    def total_count(self) -> int:
        return len(self.items)

    @property
    def is_complete(self) -> bool:
        return bool(self.items) and self.completed_count == self.total_count

    @property
    def next_item(self) -> ChecklistItem | None:
        return next((item for item in self.items if not item.completed), None)


@dataclass(frozen=True)
class ExportTask:
    """A normalized Trellis task ready for markdown rendering."""

    task_dir: Path
    source_json_path: Path
    title: str
    description: str
    status: str
    priority: str
    assignee: str
    dev_type: str
    scope: str
    created_at: str
    completed_at: str
    prd_path: Path | None
    design_path: Path | None
    implement_path: Path | None
    checklist_source: str
    checklist_sections: tuple[ChecklistSection, ...]

    @property
    def status_tag(self) -> str:
        if self.status in STATUS_LABELS:
            return self.status.strip().replace("_", "-")
        return "unknown"

    @property
    def status_label(self) -> str:
        return STATUS_LABELS.get(self.status, f"其他（{self.status or 'unknown'}）")

    @property
    def checkbox(self) -> str:
        return "x" if self.status == "completed" else " "

    @property
    def checklist_completed_count(self) -> int:
        return sum(section.completed_count for section in self.checklist_sections)

    @property
    def checklist_total_count(self) -> int:
        return sum(section.total_count for section in self.checklist_sections)

    @property
    def current_checklist_section(self) -> ChecklistSection | None:
        return next((section for section in self.checklist_sections if not section.is_complete), None)

    @property
    def phase(self) -> str:
        if self.status == "completed":
            return "completed"
        if self.status == "planning":
            return "planning"
        if self.status == "blocked":
            return "blocked"
        if self.status != "in_progress":
            return "unknown"

        current_section = self.current_checklist_section
        if current_section is None:
            return "finishing"
        normalized = current_section.title.casefold()
        if any(hint in normalized for hint in FINISH_SECTION_HINTS):
            return "finishing"
        return "executing"

    @property
    def phase_label(self) -> str:
        return {
            "planning": "Phase 1 · 规划",
            "executing": "Phase 2 · 执行",
            "finishing": "Phase 3 · 收尾",
            "blocked": "受阻",
            "completed": "已完成",
            "unknown": "状态未知",
        }[self.phase]


@dataclass(frozen=True)
class DetailProjection:
    """One vault-internal, read-only mirror for a Trellis task."""

    path: Path
    vault_link: str
    content: str


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", help="Repository root; auto-detected by default.")
    parser.add_argument("--config", help="Alternate hooks.local.json path for tests.")
    parser.add_argument("--vault-path", help="Override obsidian.vault_path.")
    parser.add_argument("--task-dashboard-dir", help="Override obsidian.task_dashboard_dir.")
    parser.add_argument("--language", help="Override obsidian.language.")
    parser.add_argument(
        "--output",
        help="Override output markdown path (must stay inside the configured dashboard directory).",
    )
    parser.add_argument("--dry-run", action="store_true", help="Render and validate without writing.")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Exit 0 only when the existing exported note already matches the generated content.",
    )
    parser.add_argument("--stdout", action="store_true", help="Print the generated markdown to stdout.")
    return parser.parse_args()


def _warn(message: str) -> None:
    print(f"[WARN] {message}", file=sys.stderr)


def _slugify_assignee(value: str) -> str:
    slug = []
    previous_dash = False
    for char in value.strip().lower():
        if char.isalnum():
            slug.append(char)
            previous_dash = False
            continue
        if not previous_dash:
            slug.append("-")
            previous_dash = True
    return "".join(slug).strip("-")


def _wikilink(target: str, label: str, *, heading: str = "") -> str:
    """Build an Obsidian-internal link with conservative label escaping."""
    safe_label = label.replace("|", "｜").replace("]", "］")
    anchor = f"#{heading}" if heading else ""
    return f"[[{target}{anchor}|{safe_label}]]"


def _detail_stem(task: ExportTask) -> str:
    """Return a filesystem- and wikilink-safe detail-note stem."""
    stem = re.sub(r"[^A-Za-z0-9._-]+", "-", task.task_dir.name).strip(".-")
    if stem:
        return stem
    return f"task-{sha256(str(task.source_json_path).encode('utf-8')).hexdigest()[:12]}"


def _compact_text(value: str, *, limit: int = 120) -> str:
    """Collapse whitespace and keep generated Task Board cards readable."""
    compact = " ".join(value.split())
    if len(compact) <= limit:
        return compact
    return compact[: limit - 1].rstrip() + "…"


def _clean_heading(value: str) -> str:
    return re.sub(r"\s+#+\s*$", "", value).strip()


def _extract_checklist(path: Path | None) -> tuple[ChecklistSection, ...]:
    """Extract Markdown checkboxes without modifying their source document."""
    if path is None:
        return ()
    try:
        content = path.read_text(encoding="utf-8")
    except OSError as exc:
        _warn(f"Unable to read workflow checklist at {path}: {exc}")
        return ()

    current_heading = "清单"
    ordered_titles: list[str] = []
    grouped: dict[str, list[ChecklistItem]] = {}
    for line in content.splitlines():
        heading_match = HEADING_PATTERN.match(line)
        if heading_match:
            current_heading = _clean_heading(heading_match.group(2)) or "清单"
            continue

        checkbox_match = CHECKBOX_PATTERN.match(line)
        if not checkbox_match:
            continue
        if current_heading not in grouped:
            ordered_titles.append(current_heading)
            grouped[current_heading] = []
        grouped[current_heading].append(
            ChecklistItem(
                text=_compact_text(checkbox_match.group(2)),
                completed=checkbox_match.group(1).casefold() == "x",
            )
        )

    return tuple(
        ChecklistSection(title=title, items=tuple(grouped[title]))
        for title in ordered_titles
    )


def _resolve_task_artifact(tasks_root: Path, path: Path) -> Path | None:
    if not path.is_file():
        return None
    return _resolve_within_tasks_root(tasks_root, path)


def _workflow_checklist(
    implement_path: Path | None,
    prd_path: Path | None,
) -> tuple[str, tuple[ChecklistSection, ...]]:
    implement_sections = _extract_checklist(implement_path)
    if implement_sections:
        return "执行清单", implement_sections
    prd_sections = _extract_checklist(prd_path)
    if prd_sections:
        return "验收清单", prd_sections
    return "", ()


def _resolve_task_child_indent(config: ObsidianConfig) -> str:
    """Match Task Board's indentation rule from Obsidian's app.json."""
    app_config_path = config.vault_path / ".obsidian" / "app.json"
    if not app_config_path.is_file():
        return DEFAULT_TASK_CHILD_INDENT
    try:
        payload = json.loads(app_config_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        _warn(f"Unable to read Obsidian indentation settings at {app_config_path}: {exc}")
        return DEFAULT_TASK_CHILD_INDENT
    if not isinstance(payload, dict) or payload.get("useTab") is not False:
        return DEFAULT_TASK_CHILD_INDENT
    tab_size = payload.get("tabSize", 4)
    if not isinstance(tab_size, int) or isinstance(tab_size, bool) or tab_size < 1:
        tab_size = 4
    return " " * tab_size


def _read_task_json(path: Path) -> dict | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        _warn(f"Skipping unreadable task JSON at {path}: {exc}")
        return None


def _resolve_within_tasks_root(tasks_root: Path, candidate: Path) -> Path | None:
    """Resolve a task path while rejecting symlink escapes out of tasks_root."""
    resolved_root = tasks_root.resolve()
    try:
        resolved_candidate = candidate.resolve()
        resolved_candidate.relative_to(resolved_root)
    except (OSError, ValueError) as exc:
        _warn(f"Skipping task path outside .trellis/tasks via symlink escape: {candidate} ({exc})")
        return None
    return resolved_candidate


def _task_sort_key(task: ExportTask) -> tuple:
    return (
        STATUS_SECTION_ORDER.get(task.status, 99),
        PRIORITY_ORDER.get(task.priority, 99),
        task.created_at or "",
        task.title.casefold(),
        task.task_dir.as_posix(),
    )


def _iter_task_json_paths(tasks_dir: Path) -> Iterable[Path]:
    if not tasks_dir.is_dir():
        return

    for child in sorted(tasks_dir.iterdir(), key=lambda entry: entry.name):
        if child.name == DIR_ARCHIVE or not child.is_dir():
            continue
        yield child / FILE_TASK_JSON

    archive_dir = tasks_dir / DIR_ARCHIVE
    if not archive_dir.is_dir():
        return
    for path in sorted(archive_dir.rglob(FILE_TASK_JSON), key=lambda entry: entry.as_posix()):
        yield path


def collect_tasks(repo_root: Path) -> list[ExportTask]:
    """Collect active + archived Trellis tasks without mutating source files."""
    tasks_dir = repo_root / ".trellis" / DIR_TASKS
    resolved_tasks_dir = tasks_dir.resolve()
    tasks: list[ExportTask] = []
    seen_dirs: set[Path] = set()

    for task_json_path in _iter_task_json_paths(tasks_dir):
        resolved_task_json_path = _resolve_within_tasks_root(resolved_tasks_dir, task_json_path)
        if resolved_task_json_path is None:
            continue
        task_dir = resolved_task_json_path.parent
        if task_dir in seen_dirs:
            continue
        seen_dirs.add(task_dir)

        payload = _read_task_json(resolved_task_json_path)
        if not isinstance(payload, dict):
            continue

        title = str(payload.get("title") or payload.get("name") or task_dir.name)
        description = str(payload.get("description") or "").strip()
        status = str(payload.get("status") or "").strip() or "unknown"
        priority = str(payload.get("priority") or "").strip()
        assignee = str(payload.get("assignee") or "").strip()
        dev_type = str(payload.get("dev_type") or "").strip()
        scope = str(payload.get("scope") or "").strip()
        created_at = str(payload.get("createdAt") or "").strip()
        completed_at = str(payload.get("completedAt") or "").strip()

        resolved_prd = _resolve_task_artifact(resolved_tasks_dir, task_dir / "prd.md")
        if resolved_prd is None:
            _warn(f"Task is missing prd.md; rendering without PRD link: {task_dir}")

        resolved_design = _resolve_task_artifact(resolved_tasks_dir, task_dir / "design.md")
        resolved_implement = _resolve_task_artifact(
            resolved_tasks_dir,
            task_dir / "implement.md",
        )
        checklist_source, checklist_sections = _workflow_checklist(
            resolved_implement,
            resolved_prd,
        )

        tasks.append(
            ExportTask(
                task_dir=task_dir,
                source_json_path=resolved_task_json_path,
                title=title,
                description=description,
                status=status,
                priority=priority,
                assignee=assignee,
                dev_type=dev_type,
                scope=scope,
                created_at=created_at,
                completed_at=completed_at,
                prd_path=resolved_prd,
                design_path=resolved_design,
                implement_path=resolved_implement,
                checklist_source=checklist_source,
                checklist_sections=checklist_sections,
            )
        )

    return sorted(tasks, key=_task_sort_key)


def _workflow_lines(task: ExportTask) -> list[tuple[bool, str]]:
    if task.phase == "completed":
        return [
            (True, "Workflow · Phase 1 规划"),
            (True, "Workflow · Phase 2 执行"),
            (True, "Workflow · Phase 3 收尾"),
        ]
    if task.phase == "planning":
        return [
            (False, "Workflow · Phase 1 规划（当前）"),
            (False, "Workflow · Phase 2 执行（待开始）"),
            (False, "Workflow · Phase 3 收尾（待开始）"),
        ]
    if task.phase == "executing":
        return [
            (True, "Workflow · Phase 1 规划"),
            (False, "Workflow · Phase 2 执行（当前）"),
            (False, "Workflow · Phase 3 收尾（待开始）"),
        ]
    if task.phase == "finishing":
        return [
            (True, "Workflow · Phase 1 规划"),
            (True, "Workflow · Phase 2 执行"),
            (False, "Workflow · Phase 3 收尾（当前）"),
        ]
    if task.phase == "blocked":
        return [(False, "Workflow · 当前受阻；解除阻塞后继续原阶段")]
    return [(False, f"Workflow · 未识别状态：{task.status}")]


def _checklist_lines(task: ExportTask) -> list[tuple[bool, str]]:
    """Render source checklists as read-only Task Board subtasks."""
    total = task.checklist_total_count
    if task.status == "completed" or total == 0:
        return []

    if total <= MAX_INLINE_CHECKLIST_ITEMS:
        details: list[tuple[bool, str]] = []
        include_section = len(task.checklist_sections) > 1
        for section in task.checklist_sections:
            for item in section.items:
                prefix = task.checklist_source
                if include_section:
                    prefix += f" · {section.title}"
                details.append(
                    (
                        item.completed,
                        _compact_text(f"{prefix} · {item.text}", limit=180),
                    )
                )
        return details

    current_section = task.current_checklist_section
    summaries: list[tuple[bool, str]] = []
    for section in task.checklist_sections:
        summary = (
            f"{task.checklist_source} · {section.title} · "
            f"{section.completed_count}/{section.total_count}"
        )
        if section == current_section and section.next_item is not None:
            summary += f"；下一步：{section.next_item.text}"
        summaries.append((section.is_complete, _compact_text(summary, limit=200)))
    return summaries


def _header_progress(task: ExportTask) -> str:
    parts = [f"阶段：{task.phase_label}"]
    if task.status != "completed" and task.checklist_total_count:
        parts.append(
            f"{task.checklist_source}："
            f"{task.checklist_completed_count}/{task.checklist_total_count}"
        )
    return " · ".join(parts)


def _read_projection_source(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8").rstrip()
    except OSError as exc:
        raise ObsidianConfigError(f"Failed to read Trellis document {path}: {exc}") from exc


def _detail_document_section(
    lines: list[str],
    *,
    heading: str,
    path: Path | None,
    missing_message: str,
) -> None:
    lines.extend(["", f"## {heading}", ""])
    if path is None:
        lines.append(missing_message)
        return
    lines.extend(
        [
            f"来源：`{path}`",
            "",
            _read_projection_source(path),
        ]
    )


def render_task_detail(repo_root: Path, task: ExportTask) -> str:
    """Render a readable vault-local mirror of one Trellis task."""
    lines = [
        "---",
        f"generated_by: {DETAIL_GENERATED_BY}",
        "readonly: true",
        f"project: {json.dumps(repo_root.name, ensure_ascii=False)}",
        f"title: {json.dumps(task.title, ensure_ascii=False)}",
        f"trellis_status: {json.dumps(task.status, ensure_ascii=False)}",
        "cssclasses:",
        "  - trellis-task-detail",
        "---",
        "",
        f"# {task.title}",
        "",
        "> [!info] 自动生成 · 只读详情",
        "> 此页是 Trellis 源文档在 Obsidian 库内的展示镜像，解决库外 `file://` 链接无法打开的问题。",
        "> 请在 Trellis 项目中修改任务；下一次同步会用权威源重新生成此页。",
        "",
        f"- 状态：{task.status_label}",
        f"- 当前阶段：{task.phase_label}",
    ]
    if task.checklist_total_count:
        lines.append(
            f"- {task.checklist_source}："
            f"{task.checklist_completed_count}/{task.checklist_total_count}"
        )
    if task.priority:
        lines.append(f"- 优先级：{task.priority}")
    if task.assignee:
        lines.append(f"- 负责人：{task.assignee}")
    if task.dev_type:
        lines.append(f"- 类型：{task.dev_type}")
    if task.scope:
        lines.append(f"- 范围：{task.scope}")
    if task.created_at:
        lines.append(f"- 创建时间：{task.created_at}")
    if task.completed_at:
        lines.append(f"- 完成时间：{task.completed_at}")
    lines.append(f"- Trellis Task：`{task.task_dir}`")
    if task.description:
        lines.extend(["", "## 摘要", "", task.description])

    _detail_document_section(
        lines,
        heading="PRD",
        path=task.prd_path,
        missing_message="此 Task 没有 `prd.md`。",
    )
    _detail_document_section(
        lines,
        heading="设计",
        path=task.design_path,
        missing_message="此 Task 没有 `design.md`。",
    )
    _detail_document_section(
        lines,
        heading="实施计划",
        path=task.implement_path,
        missing_message="此 Task 没有 `implement.md`。",
    )

    task_json = _read_projection_source(task.source_json_path)
    lines.extend(
        [
            "",
            "## Task JSON",
            "",
            f"来源：`{task.source_json_path}`",
            "",
            "```json",
            task_json,
            "```",
        ]
    )
    return "\n".join(lines) + "\n"


def _build_detail_projections(
    repo_root: Path,
    config: ObsidianConfig,
    tasks: list[ExportTask],
) -> list[DetailProjection]:
    """Resolve all detail outputs before any write, including collision checks."""
    base_stems = [_detail_stem(task) for task in tasks]
    stem_counts: dict[str, int] = {}
    for stem in base_stems:
        stem_counts[stem] = stem_counts.get(stem, 0) + 1

    projections: list[DetailProjection] = []
    seen_paths: set[Path] = set()
    for task, base_stem in zip(tasks, base_stems, strict=True):
        stem = base_stem
        if stem_counts[base_stem] > 1:
            try:
                identity = task.source_json_path.relative_to(repo_root).as_posix()
            except ValueError:
                identity = task.source_json_path.as_posix()
            stem += f"-{sha256(identity.encode('utf-8')).hexdigest()[:8]}"
        path = resolve_dashboard_output_path(
            config,
            f"{DETAIL_DIRNAME}/{stem}.md",
        )
        if path in seen_paths:
            raise ObsidianConfigError(f"Generated detail-note path collision: {path}")
        seen_paths.add(path)
        vault_link = path.relative_to(config.vault_path).with_suffix("").as_posix()
        projections.append(
            DetailProjection(
                path=path,
                vault_link=vault_link,
                content=render_task_detail(repo_root, task),
            )
        )
    return projections


def render_markdown(
    repo_root: Path,
    tasks: list[ExportTask],
    *,
    child_indent: str = DEFAULT_TASK_CHILD_INDENT,
    detail_links: dict[Path, str] | None = None,
) -> str:
    """Render the deterministic Obsidian dashboard note."""
    lines = [
        "---",
        f"generated_by: {GENERATED_BY}",
        "readonly: true",
        f"project: {repo_root.name}",
        "cssclasses:",
        "  - trellis-task-dashboard-source",
        "---",
        "",
        "> [!info] 自动生成 · 只读",
        "> 该文件由 Trellis 生命周期钩子生成，Obsidian 仅用于展示。",
        "> 请勿直接在这里修改任务；Trellis 任务目录仍是唯一权威来源。",
        "",
        "# Trellis Task 数据源",
        "",
        "此文件供 Tasks 查询与 Task Board 只读消费，覆盖范围仅限自动生成任务镜像。",
    ]

    if not tasks:
        lines.extend(["", "当前没有可导出的 Trellis 任务。"])
        return "\n".join(lines) + "\n"

    grouped: dict[str, list[ExportTask]] = {}
    for task in tasks:
        grouped.setdefault(task.status, []).append(task)

    for status, grouped_tasks in sorted(
        grouped.items(),
        key=lambda item: (STATUS_SECTION_ORDER.get(item[0], 99), item[0]),
    ):
        label = STATUS_LABELS.get(status, f"其他（{status}）")
        lines.extend(["", f"## {label}", ""])
        for task in grouped_tasks:
            title = task.title
            detail_target = (detail_links or {}).get(task.source_json_path)
            if detail_target is not None:
                title = _wikilink(
                    detail_target,
                    task.title,
                    heading="PRD" if task.prd_path is not None else "",
                )
            header_parts = [f"- [{task.checkbox}] {title} · {_header_progress(task)}"]
            priority_emoji = PRIORITY_EMOJI.get(task.priority)
            if priority_emoji:
                header_parts.append(priority_emoji)
            header_parts.append("#trellis")
            header_parts.append(GENERATED_SOURCE_TAG)
            header_parts.append(f"#trellis/status/{task.status_tag}")
            if task.priority:
                header_parts.append(f"#trellis/priority/{task.priority.lower()}")
            if task.assignee:
                assignee_slug = _slugify_assignee(task.assignee)
                if assignee_slug:
                    header_parts.append(f"#trellis/assignee/{assignee_slug}")
            if task.created_at:
                header_parts.append(f"➕ {task.created_at}")
            if task.completed_at:
                header_parts.append(f"✅ {task.completed_at}")
            lines.append(" ".join(header_parts))

            for completed, text in [*_workflow_lines(task), *_checklist_lines(task)]:
                lines.append(f"{child_indent}- [{'x' if completed else ' '}] {text}")

            metadata_parts = [f"状态：{task.status_label}"]
            if task.assignee:
                metadata_parts.append(f"负责人：{task.assignee}")
            if task.priority:
                metadata_parts.append(f"优先级：{task.priority}")
            if task.dev_type:
                metadata_parts.append(f"类型：{task.dev_type}")
            if task.scope:
                metadata_parts.append(f"范围：{task.scope}")
            links: list[str] = []
            if detail_target is not None:
                if task.prd_path is not None:
                    links.append(_wikilink(detail_target, "PRD", heading="PRD"))
                if task.design_path:
                    links.append(_wikilink(detail_target, "设计", heading="设计"))
                if task.implement_path:
                    links.append(_wikilink(detail_target, "实施计划", heading="实施计划"))
                links.append(_wikilink(detail_target, "Task JSON", heading="Task JSON"))
            if links:
                metadata_parts.append("文档：" + " · ".join(links))
            lines.append(child_indent + "；".join(metadata_parts))
            if task.description:
                lines.append(
                    child_indent + f"摘要：{_compact_text(task.description, limit=220)}"
                )

    return "\n".join(lines) + "\n"


def _has_expected_generated_frontmatter(
    content: str,
    *,
    generated_by: str = GENERATED_BY,
) -> bool:
    lines = content.splitlines()
    if not lines or lines[0].strip() != "---":
        return False
    try:
        closing_index = next(
            index for index, line in enumerate(lines[1:], start=1) if line.strip() == "---"
        )
    except StopIteration:
        return False
    return f"generated_by: {generated_by}" in {
        line.strip() for line in lines[1:closing_index]
    }


def _is_safe_generated_file(
    path: Path,
    *,
    generated_by: str = GENERATED_BY,
) -> bool:
    if not path.exists():
        return True
    try:
        content = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise ObsidianConfigError(f"Failed to read existing generated file {path}: {exc}") from exc
    return _has_expected_generated_frontmatter(content, generated_by=generated_by)


def _content_hash(text: str) -> str:
    return sha256(text.encode("utf-8")).hexdigest()


def export_dashboard(
    repo_root: Path,
    config: ObsidianConfig,
    *,
    output_override: str | None = None,
    dry_run: bool = False,
    check: bool = False,
) -> tuple[Path, str, bool]:
    """Generate and optionally write the dashboard plus detail mirrors."""
    tasks = collect_tasks(repo_root)
    output_path = resolve_dashboard_output_path(
        config,
        OUTPUT_FILENAME,
        output_path=output_override,
    )
    detail_projections = _build_detail_projections(repo_root, config, tasks)
    detail_paths = {projection.path for projection in detail_projections}
    if output_path in detail_paths:
        raise ObsidianConfigError(
            f"Dashboard output collides with a generated detail note: {output_path}"
        )
    rendered = render_markdown(
        repo_root,
        tasks,
        child_indent=_resolve_task_child_indent(config),
        detail_links={
            task.source_json_path: projection.vault_link
            for task, projection in zip(tasks, detail_projections, strict=True)
        },
    )

    generated_outputs = [
        *(
            (projection.path, projection.content, DETAIL_GENERATED_BY)
            for projection in detail_projections
        ),
        (output_path, rendered, GENERATED_BY),
    ]

    # Preflight every owned output before writing any of them. A colliding user
    # note therefore cannot leave the dashboard in a partially updated state.
    for path, _, generated_by in generated_outputs:
        if not _is_safe_generated_file(path, generated_by=generated_by):
            raise ObsidianConfigError(
                f"Refusing to overwrite non-generated file: {path}"
            )

    changed_outputs: list[tuple[Path, str]] = []
    for path, content, _ in generated_outputs:
        current = ""
        if path.exists():
            try:
                current = path.read_text(encoding="utf-8")
            except OSError as exc:
                raise ObsidianConfigError(
                    f"Failed to read existing generated file {path}: {exc}"
                ) from exc
        if _content_hash(current) != _content_hash(content):
            changed_outputs.append((path, content))

    changed = bool(changed_outputs)
    if check:
        return output_path, rendered, changed
    if not dry_run:
        for path, content in changed_outputs:
            atomic_write_text(path, content)
    return output_path, rendered, changed


def main() -> int:
    args = _parse_args()
    repo_root = Path(args.repo_root).resolve() if args.repo_root else get_repo_root()

    try:
        config = resolve_obsidian_config(
            repo_root,
            config_path=Path(args.config).resolve() if args.config else None,
            vault_path=args.vault_path,
            task_dashboard_dir=args.task_dashboard_dir,
            language=args.language,
        )
    except ObsidianConfigError as exc:
        _warn(str(exc))
        return 0 if not args.check else 1

    try:
        output_path, rendered, changed = export_dashboard(
            repo_root,
            config,
            output_override=args.output,
            dry_run=args.dry_run or args.stdout,
            check=args.check,
        )
    except ObsidianConfigError as exc:
        _warn(str(exc))
        return 1

    if args.stdout:
        print(rendered, end="")
    if args.check:
        if changed:
            _warn(f"Dashboard is missing or out of date: {output_path}")
            return 1
        print(f"[OK] Dashboard is up to date: {output_path}")
        return 0

    if args.dry_run:
        print(f"[DRY-RUN] {'Would update' if changed else 'No changes'}: {output_path}")
    else:
        print(f"[OK] {'Updated' if changed else 'No changes'}: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
