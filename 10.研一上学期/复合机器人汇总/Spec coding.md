
## Why Spec Coding Is Necessary

- Small scripts and simple tasks can often be completed through pure vibe coding.
- Models have limited context. A large project is too complex for a model to understand in full at all times.
- Stacking locally optimal fixes creates layers of accidental complexity and eventually makes the system difficult to maintain.
- Large projects often involve multiple developers, so they need explicit and enforceable constraints.

## Agent = Model + Harness

- A harness is the engineering environment around a model: coding conventions, workflow constraints, accumulated experience, bug records, validation steps, and guardrails.
- The model provides reasoning and generation; the harness makes its behavior repeatable and project-aware.

## Rules Files

Typical examples include `AGENTS.md`, `CLAUDE.md`, and similar platform-specific files.

- Different agent tools use different rule files, which makes team-wide governance difficult.
- As a project grows, a single rules file also grows and can become bloated.
- Rules files are usually static. Lessons from implementation and debugging are not preserved unless someone deliberately writes them back.

## Skills

- A skill packages a repeatable workflow, such as debugging, writing tests, reviewing code, or finishing a task.
- Skills describe *how to perform a class of work*, while specs describe *how code in this project must be written*.
- Different AI platforms may use different skill formats, even when the underlying workflow is the same.

## Advantages of Trellis

- It reduces the need to explain the project again in every session by keeping project knowledge under `.trellis/`.
- It preserves task records and working memory, making it possible to continue from previous work when a similar problem appears.
- It is cross-platform: Claude Code, Cursor, Codex, and other supported tools can share the same `.trellis/` knowledge base.
- It provides a complete project feedback loop:
  - **Spec** — long-lived conventions such as naming, interface formats, error handling, testing requirements, and architectural constraints.
  - **Task** — the current unit of work, including requirements, design, implementation plan, and research.
  - **Workflow** — the current development phase and its required actions. A large deliverable can be split into independently managed tasks.
  - **Journal** — working memory: files changed, problems encountered, attempted solutions, decisions made, and where to continue next time.
  - **Spec update** — at the end of a task, reusable and stable lessons are selected and promoted into the spec.

In short: **Spec constrains how a Task is performed; the Task advances through a Workflow; the Journal records the process; and stable lessons are promoted back into Spec.** This creates controlled, long-term evolution.

## Trellis Structure

```text
your-project/
├── .trellis/                         # Cross-platform Trellis core
│   ├── .developer                    # Local developer identity
│   ├── .version                      # Trellis version
│   ├── .template-hashes.json         # Template version and local-edit tracking
│   ├── workflow.md                   # Plan → Execute → Finish workflow
│   ├── config.yaml                   # Project configuration
│   │
│   ├── .runtime/                     # Runtime state; normally not committed
│   │   └── sessions/
│   │       └── <session-key>.json    # Active task for the current session
│   │
│   ├── spec/                         # Long-lived project conventions
│   │   ├── frontend/                 # Frontend conventions
│   │   ├── backend/                  # Backend conventions
│   │   └── guides/                   # Cross-layer thinking and design guides
│   │
│   ├── workspace/                    # Per-developer working memory
│   │   ├── index.md
│   │   └── <developer-name>/
│   │       ├── index.md
│   │       └── journal-N.md          # Session logs
│   │
│   ├── tasks/                        # Task directories
│   │   ├── <MM-DD-task-name>/        # Current task
│   │   │   ├── task.json             # Status, owner, branch, and metadata
│   │   │   ├── prd.md                # Requirements, scope, and acceptance criteria
│   │   │   ├── design.md             # Technical design and trade-offs
│   │   │   ├── implement.md          # Ordered implementation plan
│   │   │   ├── implement.jsonl       # Context required during implementation
│   │   │   ├── check.jsonl           # Context required during review
│   │   │   └── research/             # Research evidence
│   │   └── archive/                  # Completed tasks
│   │
│   └── scripts/                      # Trellis automation scripts
│       ├── task.py                   # Task management
│       ├── get_context.py            # Project-context loading
│       ├── add_session.py            # Session journaling
│       ├── create_bootstrap.py       # Spec bootstrapping
│       └── common/                   # Shared script modules
│
├── .claude/                          # Claude Code integration layer
│   ├── settings.json
│   ├── commands/trellis/             # Trellis commands
│   ├── agents/                       # Implement/check/research agents
│   ├── skills/                       # Brainstorm/check/update-spec skills
│   └── hooks/                        # Session and context-injection hooks
│
├── .cursor/                          # Cursor integration layer
├── .codex/                           # Codex integration layer
├── .opencode/                        # OpenCode integration layer
└── AGENTS.md                         # Project entry instructions for Codex and similar tools
```

## Lifecycle of a Task

```text
Create Task
    ↓
Bind it to the current session
    ↓
planning: requirements and design
    ↓
in_progress: implementation and checking
    ↓
Complete the code commit
    ↓
Run finish-work
    ↓
Archive the task and clear the active-task pointer
```

Once a task is created, Trellis creates a dedicated directory for it. Requirements, design, implementation plans, context manifests, and research are stored there instead of remaining only in chat history.

## Trellis Workflow

### Step 1: Restore Project Context at the Start of a Session

| Item | Meaning |
| --- | --- |
| Developer | Identifies the current developer and locates `workspace/<developer>/` and the personal journal. |
| Git state | Loads the current branch, uncommitted files, and recent commits so the AI understands the repository state. |
| Task session ID | Maps the current session to its active task directory. |
| Task status | Determines whether the task is in `planning`, `in_progress`, or another state. |
| Workflow index | Identifies the current phase and the next required action. |
| Spec index paths | Locates the project conventions that may need to be loaded for the task. |
| Workspace memory | Restores previous journal entries, problems, decisions, and next steps. |

```text
Who is the developer?
    ↓
What is the current repository state?
    ↓
Which task is active?
    ↓
What stage is the task in?
    ↓
What action is required next?
    ↓
Which project conventions apply?
    ↓
Where did the previous session stop?
```

### Step 2: Inject Workflow State on Each Turn

Each prompt receives a compact description of the active task, workflow state, and next action. The AI then loads the required PRD, design, implementation plan, specs, research, and journal details.

### Step 3: Decide Whether a Task Is Necessary

Trellis does not need to create a task for every interaction. A small question or a read-only technical explanation can be handled directly, while implementation work should be classified by risk and recorded at an appropriate level.

### Step 4: Analyze Requirements Before Planning

Before implementation, Trellis asks the questions needed to remove ambiguity. For a login feature, the artifacts might look like this:

| File | Main question | Example |
| --- | --- | --- |
| `prd.md` | What must be built, and what counts as complete? | Support account/password login; return a unified error format; expire tokens after two hours. |
| `design.md` | How will it be built, and why? | Add a login endpoint, JWT middleware, user-table fields, and explicit module relationships. |
| `implement.md` | In what order will the work be executed? | Change the database, implement the endpoint, add authentication middleware, then add tests. |
| `research/` | What was discovered, and what evidence supports it? | Existing JWT library, version compatibility, and the current user-table structure. |

### Step 5: Implement with the Full Task Context

After planning is complete, the task enters `in_progress`. The AI does not write code from the latest prompt alone; it uses the task status, task artifacts, relevant specs, research, and the actual repository code.

### Step 6: Check the Result Against the Same Contracts

During the check phase, the AI reviews the diff, runs appropriate validation, and loads the specs and research referenced by `check.jsonl`. This turns written conventions into review constraints.

### Step 7: Capture Reusable Lessons

Writing every observation into the spec would make it bloated and less useful. The update-spec step must filter lessons and promote only stable, reusable, project-specific knowledge.

### Step 8: Finish and Archive the Task

After the code is committed, the developer triggers the platform's `finish-work` flow. Trellis then:

- moves `.trellis/tasks/<task>/` into `archive/`;
- appends a summary to `.trellis/workspace/<developer>/journal-N.md`;
- clears the active-task pointer.

## Recommended Division of Work

```text
Developer describes the requirement
        ↓
AI asks questions and creates prd.md
        ↓
Developer confirms requirements and acceptance criteria
        ↓
AI creates the design and implementation plan
        ↓
AI implements, checks, and tests
        ↓
Developer reviews the diff and key decisions
        ↓
Developer commits the code
        ↓
Developer triggers finish-work; AI archives the task and updates the journal
        ↓
Developer/team reviews whether any lesson should be promoted into Spec
```

## How to Add Coding Conventions

### 1. Choose the Correct Destination

Use a code spec when the rule tells the AI **how to write or validate code**. Use a guide when it tells the AI **what to think about before making a decision**.

| Convention type | Recommended location in MFMS |
| --- | --- |
| Server architecture, database, errors, logging | `.trellis/spec/backend/` |
| Qt UI, client boundaries, state, type safety | `.trellis/spec/frontend/` |
| ROS messages/services and compatibility | `.trellis/spec/interfaces/` |
| Robot adapters, SDK boundaries, real-device safety | `.trellis/spec/adapters/` |
| Cross-layer reasoning and safety checklists | `.trellis/spec/guides/` |

> [!warning]
> Do not place project-private conventions in a bundled Trellis skill or in a global installation directory. Put them in the repository's `.trellis/spec/` or in a project-local skill.

### 2. Inspect the Existing Spec and Real Code

Before writing a rule:

```bash
python3 ./.trellis/scripts/get_context.py --mode packages
```

- Read the target layer's `index.md` and related topic files.
- Inspect representative production code and tests.
- Confirm that the proposed rule describes the current intended architecture, not an abandoned design or a one-off workaround.
- Search for an existing equivalent rule to avoid duplication.

### 3. Write an Executable Convention

A useful convention should state:

1. **Scope / Trigger** — when the rule applies.
2. **Signatures** — relevant API, command, ROS, database, or class interfaces.
3. **Contracts** — fields, types, units, ownership, threading, timeouts, and boundary behavior.
4. **Validation and errors** — invalid inputs, failure behavior, and error propagation.
5. **Good, base, and bad cases** — normal use, boundary use, and prohibited use.
6. **Tests required** — test level and exact assertion points.
7. **Wrong vs. correct** — at least one concrete pair.

Example:

````markdown
## Scenario: Propagating a Robot Command Failure

### Scope / Trigger

Applies whenever a command crosses `CommunicationInterface -> Worker -> Gateway -> adapter`.

### Contract

- Preserve `requestId`, device ID, command name, and the lower-layer error code.
- Never replace a device timeout with a generic success or an empty result.

### Validation and Error Matrix

| Condition | Required result |
| --- | --- |
| Unknown device ID | Reject before SDK invocation and identify the device. |
| SDK timeout | Return timeout status and preserve the request ID. |
| Device rejects command | Return the vendor error code through the public result. |

### Tests Required

- Unit test: unknown device is rejected before the adapter call.
- Integration test: adapter timeout reaches the client with the same request ID.

### Wrong vs. Correct

```cpp
// Wrong: context is discarded.
return Result::failure("command failed");

// Correct: preserve the operational context.
return Result::failure(device_id, command, request_id, vendor_code, message);
```
````

### 4. Update the Layer Index

If a new spec file is created, add it to the corresponding `index.md`. Also add any pre-development or quality-check item needed to make the rule discoverable and enforceable.

```markdown
## File Index

- [error-handling.md](error-handling.md)
```

### 5. Attach the Spec to the Current Task

Writing a spec is not enough; the task must load it during implementation and review.

```bash
python3 ./.trellis/scripts/task.py current --source

python3 ./.trellis/scripts/task.py add-context <task> implement \
  ".trellis/spec/backend/error-handling.md" \
  "Error propagation contract"

python3 ./.trellis/scripts/task.py add-context <task> check \
  ".trellis/spec/backend/error-handling.md" \
  "Review error propagation"

python3 ./.trellis/scripts/task.py validate <task>
```

### 6. Review Spec Changes Like Code

- Keep spec changes focused and traceable.
- Review important conventions through a pull request.
- Require evidence from source code, tests, a confirmed design decision, or a reproduced bug.
- Update or remove obsolete rules when architecture changes.

## How Trellis Can Self-Learn Coding Conventions

> [!important] What “self-learning” means
> Trellis does not train the model or change its weights. Its durable learning is an external knowledge loop: evidence from one task is distilled into `.trellis/spec/`, then loaded and enforced in later tasks.

### The Controlled Learning Loop

```text
Implementation / bug / review finding
                ↓
Record evidence in the task and journal
                ↓
Extract a candidate convention
                ↓
Filter for stability, reuse, and project specificity
                ↓
Write or update an executable code spec
                ↓
Link it from index.md
                ↓
Inject it into future implement/check context
                ↓
Validate against it during review and tests
                ↓
Use new failures as feedback for the next revision
```

### 1. Collect Learning Candidates

Candidates usually come from:

- a bug whose root cause could recur;
- a design decision that future code must preserve;
- a repeated review comment;
- a non-obvious SDK, ROS, database, Qt, or threading constraint;
- a new interface contract or error behavior;
- a test that reveals a missing invariant;
- a pattern that succeeds in several related modules.

Task artifacts and journals are evidence stores. They should not automatically become specs.

### 2. Apply a Promotion Gate

Promote a lesson only when most of the following are true:

- **Reusable** — it is likely to affect future work.
- **Verified** — it is supported by code, tests, documentation, or a confirmed team decision.
- **Project-specific** — it adds more value than a generic best practice.
- **Actionable** — a developer or AI can follow and check it.
- **Stable** — it is not merely an experiment or temporary workaround.
- **Cost-effective** — preserving it prevents meaningful defects or repeated investigation.

Do not promote chat summaries, speculative ideas, one-off details, or unverified guesses.

### 3. Run the Update-Spec Step After Checking

The recommended finish sequence is:

```text
implement → check → update-spec → commit → finish-work
```

In this project, the AI-facing skill is stored at:

```text
.agents/skills/trellis-update-spec/SKILL.md
```

Ask the AI to run `trellis-update-spec` after implementation and checking. For recurring bug classes, run `trellis-break-loop` first to identify the prevention rule, then use `trellis-update-spec` to write it into the correct layer.

### 4. Make Learning a Workflow Gate

Keep a required spec-review step in `.trellis/workflow.md` before commit and finish-work. The gate should require the AI to:

- inspect the diff, tests, review findings, and task decisions;
- list candidate learnings;
- reject one-off or speculative items;
- update the relevant spec and its index when a candidate passes;
- check that the updated rule contains examples, error behavior, and test assertions;
- report “no spec update needed” when nothing qualifies.

A reusable instruction is:

```text
After implementation and checking, inspect the diff, test results, bug causes,
and design decisions. Identify reusable project-specific conventions. Promote
only verified and stable lessons. Write accepted lessons as executable contracts
in the relevant .trellis/spec layer, update its index, and add the spec to future
implementation and check context. If no lesson qualifies, state why.
```

### 5. Reinject Learned Rules

Learning has no effect unless future tasks read the result.

- Keep every layer's `index.md` current.
- Add relevant specs to `implement.jsonl` and `check.jsonl` during planning.
- Make pre-development skills read the applicable spec index before editing code.
- Make check agents compare the diff against the same specs.
- Keep concrete requirements testable so validation can enforce them.

### 6. Keep the Knowledge Base Healthy

- One concept per section or topic file.
- Prefer precise contracts and examples over broad principles.
- Merge duplicates and remove superseded rules.
- Keep guides short; point them to detailed code specs instead of copying the same content.
- Treat source code and verified behavior as evidence, but treat the reviewed spec as the intended contract.
- Put proposed architecture in task design documents until the team accepts it; do not present a roadmap as current behavior.

The key distinction is:

> **Journal = what happened. Task = why this change exists. Spec = the reviewed rule future work must follow.**

## Managing Trellis in a Team

### Files That Should Usually Be Committed

- `.trellis/spec/` — shared conventions; changes should receive the same review discipline as code.
- `.trellis/tasks/` — requirements, design, implementation plans, context manifests, and research are project assets.
- `.trellis/workspace/<name>/` — developer journals can preserve useful project history when the team chooses to share them.

### Files That Should Usually Remain Local

- `.trellis/.developer` — identifies the local developer and is normally ignored by Git.
- `.trellis/.runtime/` — stores session-level runtime state and is normally ignored by Git.

When specs and tasks are versioned, important API conventions, test contracts, and architectural constraints are visible to the whole team. Each developer can still keep a separate journal, preserving the real development trail without confusing it with shared standards.

Task directories may conflict when multiple people edit the same task, so assign clear ownership and avoid concurrent edits to one task directory. New team members can then inspect the specs, archived tasks, and journals to understand both the current rules and the reasons behind them.

Team conventions should evolve, but in a controlled and traceable way:

- update specs after major architectural changes;
- update specs when a new testing framework or interface contract is adopted;
- review important spec changes through pull requests;
- record the evidence and decision that justified the change;
- remove obsolete rules instead of allowing contradictions to accumulate.

This approach allows the project's engineering harness to improve continuously without turning the spec into an unreviewed dump of notes.
