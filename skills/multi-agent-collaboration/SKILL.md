---
name: multi-agent-collaboration
description: Coordinate multiple folder-scoped Codex agents working on the same project with shared ownership and logging rules. Use when the project is split into local folders or tracks, each agent must follow the root `PLAN.md`, the root `agent.md`, and the current folder documents without hardcoded project-specific path assumptions.
---

# Multi Agent Collaboration

## Overview
Use this skill when multiple agents share one repository and each agent owns a bounded folder or track.

Treat the root `PLAN.md` as the shared source of truth for requirements, architecture, and API contracts. Treat the root `agent.md` as the shared workflow and logging rules. Treat each folder's `agent.md` as the local execution brief for that folder's assigned scope.

## Document Priority
When documents overlap or conflict, use this priority:

1. Root `PLAN.md`
2. Root `agent.md`
3. Current folder `agent.md`
4. `work.md`, `error.md`, and `context.md`
5. Local `.codex/` logs

If the higher-priority document needs to change, log the issue before silently changing lower-priority documents.

## Startup Workflow
Before changing code or documents, do the following in order:

1. Read the root `PLAN.md` if it exists.
2. Read the root `agent.md` if it exists.
3. Read the root `context.md`, `work.md`, and `error.md` if they exist.
4. Read the current folder's `agent.md`, `context.md`, `work.md`, and `error.md` if they exist.
5. Inspect the current folder's `.codex/` only when summary documents are not enough.
6. Confirm the current agent's ownership boundary.
7. Identify whether the task is implementation, review, or coordination.

If local instructions conflict with the root `PLAN.md` or root `agent.md`, follow the higher-priority document and log the mismatch.

## Ownership Rules
- Work only inside the files and folders assigned to the current agent.
- Do not edit another folder's owned files unless the task explicitly requires cross-boundary integration.
- Do not invent or assume track names when the root `PLAN.md` already defines folder boundaries.
- Do not silently change shared API contracts, shared architecture rules, protocol shapes, or shared response formats from a leaf folder.

## Document Model

### Root Documents
- `PLAN.md`
  - Shared requirements
  - Shared architecture
  - Shared API specification
- `agent.md`
  - Global workflow rules
  - Logging rules
  - Ownership and escalation rules
- `context.md`
  - Conversation history summary
  - Major decisions and agreed changes
  - Important handoff context for future agents
- `work.md`
  - Overall project progress
  - Cross-track milestones
  - Integration status
- `error.md`
  - Shared blockers
  - Integration risks
  - Cross-track issues
- `.codex/`
  - Raw logs and session history

### Local Folder Documents
- `agent.md`
  - Only the requirements and scope for that folder or track
- `context.md`
  - Folder-local conversation summary
  - Handoff notes specific to that folder
- `work.md`
  - Work completed by the local agent
  - Changed files
  - Remaining blockers
- `error.md`
  - Review findings
  - Missing requirements
  - Contract mismatches
  - Risks and correction proposals
- `.codex/`
  - Raw folder-local logs and session history

## Logging Rules
- Append entries. Do not rewrite history unless asked.
- Keep entries factual and short.
- Log after meaningful work, not after every trivial read.
- Include enough detail for another agent to resume work without rereading the whole codebase.
- When the conversation produces a new decision, rule change, scope change, or handoff-critical note, append a concise summary to the relevant `context.md`.
- Use `context.md` for summary and handoff. Use `.codex/` for raw logs and session history.

## Persona Validation
- After meaningful code, contract, build, or documentation changes, run a persona-based validation pass.
- Default validation personas:
  - Correctness: verify the change matches the root `PLAN.md` and local scope.
  - Concurrency and stability: check for races, deadlocks, resource leaks, and unsafe shutdown behavior.
  - API and contract: verify protocol shape, interface compatibility, and response consistency.
  - Test and regression: verify existing coverage is preserved and new cases are not missing.
  - Operations and build: verify Docker, build steps, and reproducible local execution still make sense.
- Record findings using `Critical` for must-fix issues and `Major` for recommended fixes.
- Write cross-track findings to the root `error.md` and folder-local findings to the local `error.md`.

### Local `work.md` Entry Format
Use this format when logging completed work in the current folder:

```md
## 2026-04-22 14:30 KST - [TRACK]
- Summary: Added the local implementation needed for this folder.
- Files: src/example.c, include/example.h
- Impact: Downstream folders can now rely on the new interface.
- Blockers: Waiting on shared contract clarification from the root PLAN.md.
```

### Local `error.md` Entry Format
Use this format when reviewing another folder's work or documenting a blocking issue:

```md
## 2026-04-22 14:45 KST - Review of [TRACK]
- Severity: high
- Issue: Local implementation does not match the shared contract.
- Evidence: The folder returns a different response shape than the root PLAN.md defines.
- Impact: Other folders cannot integrate safely.
- Recommendation: Align with the root contract or update the root PLAN.md first.
```

### Root `work.md` Rule
- Update the root `work.md` only if the current agent is the coordinator or is explicitly assigned to sync global progress.
- If not assigned, write local progress to the folder `work.md` and let the coordinator roll it up.

### Root `context.md` Rule
- Use the root `context.md` to preserve important conversation outcomes, decision changes, blocked items, and agreed next steps.
- Append concise entries instead of rewriting old context unless cleanup is explicitly requested.
- Update `context.md` when a user instruction changes architecture, ownership boundaries, API contracts, naming rules, or documentation workflow.

## Implementation Workflow

### If the Current Task Is Implementation
1. Read root and local documents in priority order.
2. Confirm owned files.
3. Implement only within the local scope.
4. Log the completed work to the local `work.md`.
5. If the work affects another folder, note the dependency or impact clearly.

### If the Current Task Is Review
1. Read the target folder's `agent.md` and `work.md`.
2. Compare implementation against the root `PLAN.md`, root `agent.md`, and local requirements.
3. Write findings to the target folder's `error.md`.
4. Do not mix review findings into `work.md`.

### If the Current Task Is Coordination
1. Read all relevant local `work.md` and `error.md` files.
2. Update the root `work.md` with integrated progress.
3. Update the root `context.md` with any project-level decisions or handoff context discovered during coordination.
4. Surface cross-track blockers, dependency order, and contract mismatches.

## Change Escalation Rules
- If a local implementation requires changing shared architecture, API contracts, protocol shape, or build conventions, stop and log the issue before changing the shared rule.
- If the change is explicitly assigned, update the shared document first, then implement the code.
- If ownership is unclear, prefer logging the issue over making a silent cross-boundary edit.

## Track Mapping Rules
- Track names and folder boundaries come from the root `PLAN.md`.
- If the root `PLAN.md` does not define explicit track names, infer them from the current folder layout and avoid inventing unnecessary new folders.
- Keep examples generic unless the current project explicitly standardizes names such as `ENGINE`, `TCP-SERVER`, `CLI-CLIENT`, or `TEST`.

## Folder Layout Example
Use a structure like this when the repository is organized by track:

```text
project/
  PLAN.md
  agent.md
  context.md
  work.md
  error.md
  .codex/
  track-name/
    agent.md
    context.md
    work.md
    error.md
    .codex/
  another-track/
    agent.md
    context.md
    work.md
    error.md
    .codex/
```

## Final Check Before Finishing
Before ending the task:

1. Verify the work stayed inside the assigned ownership boundary.
2. Verify the local `work.md` was updated if meaningful progress was made.
3. Verify review findings were written to `error.md` when needed.
4. Verify no shared contract was changed silently.
5. Verify any blocker that affects another folder was written clearly enough to hand off.
