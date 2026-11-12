# AI Workflow Rules

## Approach

Build Waqt-Tray incrementally with a spec-driven workflow. The context files
define what to build (`project-overview.md`, `ui-context.md`), how
(`architecture.md`, `code-standards.md`), in what order (`build-plan.md`),
and where things stand (`progress-tracker.md`). Always implement against
these specs; do not infer or invent behaviour from scratch. `SRS.md` is
background only: where it disagrees with the context files, the context
files win.

Work phase by phase as laid out in `build-plan.md`. Phase 0 spikes are
throwaway code whose purpose is to produce **measurements**. Record the
numbers in `progress-tracker.md` before starting Phase 1.

## Verify Before You Write

Before writing code that touches any external API, library, SDK, or CLI
tool, follow `code-standards.md` → **Documentation and API Verification**:

1. Check the pinned version in the manifest.
2. Look up official docs (package CLI → Context7 via the `find-docs`
   skill → official web docs).
3. If no documentation can be retrieved, **stop and tell the user**. Do not
   write the code from memory.
4. Log the source in `progress-tracker.md` → API Verification Log.

If Context7 MCP is not configured in the agent environment, recommend it to
the user once (`context7-cli` skill → `npx ctx7@latest setup`) and continue
with the CLI or web fallback.

## Skills to Use

Project skills live in `.claude/skills/`:

| When | Skill |
| --- | --- |
| Any library/SDK/API question | `find-docs`, `context7-cli` |
| Engine, scheduler, alerts, bridge parsing (test-first, only for what `code-standards.md` → Testing lists) | `tdd` (+ `codebase-design` for seams) |
| Designing UI views | `frontend-design`, then `ui-context.md` tokens |
| Adding/styling components | `shadcn` |
| React code and bundle size | `vercel-react-best-practices` (ignore Next.js-only rules) |
| Reviewing UI | `web-design-guidelines`, `accessibility` |
| Bridge, file paths, WebView2 hardening | `security-and-hardening` |
| UI smoke tests in a browser (mock bridge) | `webapp-testing` |

## Scoping Rules

- Work on one feature unit at a time (one numbered unit in `build-plan.md`).
- Prefer small, verifiable increments over large speculative changes.
- Do not combine unrelated system boundaries in a single implementation step.

## When to Split Work

Split an implementation step if it combines:

- Native host changes and UI changes, unless the step is specifically a
  bridge message, which must land on both sides together.
- Engine math changes and scheduling/alert behaviour changes.
- Behaviour not clearly defined in the context files.
- A new third-party dependency and the feature that uses it. Vendor and
  verify the dependency first, with its licence recorded.

If a change cannot be verified end to end quickly, the scope is too broad.
Split it.

## Handling Missing Requirements

- Do not invent product behaviour not defined in the context files.
- If a requirement is ambiguous, resolve it in the relevant context file
  before implementing.
- If a requirement is missing, add it as an open question in
  `progress-tracker.md` before continuing.
- Religious-practice defaults (forbidden window lengths, method per
  region, Hijri adjustment) are always **user-configurable**. Never
  hard-code a single school's ruling without a setting.

## Protected Files

Do not modify the following unless explicitly instructed:

- `SRS.md` (original input document).
- `native/third_party/**` (vendored code; update only by re-vendoring a new
  pinned version and recording it in `VERSIONS.md`).
- `ui/src/components/ui/**` (shadcn-generated; regenerate via CLI, restyle
  via tokens).
- `native/tests/fixtures/**` (regenerate only via `scripts/gen-fixtures`).

## Keeping Docs in Sync

Update the relevant context file whenever implementation changes:

- System architecture or boundaries → `architecture.md`.
- Storage model or config schema → `architecture.md` (+ migration).
- Code conventions or standards → `code-standards.md`.
- Feature scope → `project-overview.md`.
- Plan order or units → `build-plan.md`.

## Before Moving to the Next Unit

1. The current unit works end to end within its defined scope and meets its
   acceptance criteria in `build-plan.md`.
2. No invariant in `architecture.md` was violated, and the Performance Budget
   still holds if the unit touched `native/host` (run `scripts/perf.ps1`).
3. `progress-tracker.md` reflects the completed work, with date and time,
   and API Verification Log entries for any new API patterns.
4. All of these pass: `make -C native test`,
   `bun --cwd ui run typecheck`, `bun --cwd ui run test`,
   `bun --cwd ui run build`. Commands that don't exist yet in early
   phases are skipped and noted.
