## Application Building Context

Read the following files in order before implementing
or making any architectural decision:

1. `context/project-overview.md` — product definition,
   goals, features, and scope
2. `context/ui-context.md` — theme, colors, typography,
   and component conventions
3. `context/architecture.md` — stack, system boundaries,
   storage, bridge protocol, performance budget, invariants
4. `context/code-standards.md` — implementation rules
   and conventions (incl. Documentation and API Verification)
5. `context/ai-workflow-rules.md` — development workflow,
   scoping rules, skills, and delivery approach
6. `context/build-plan.md` — phased units with acceptance
   criteria (read the current phase)
7. `context/progress-tracker.md` — current phase,
   completed work, open questions, and next steps

`SRS.md` is the original requirements input. Where it
disagrees with the context files, the context files win.

Update `context/progress-tracker.md` after each
meaningful implementation change.

If implementation changes the architecture, scope, or
standards documented in the context files, update the
relevant file before continuing.

## Testing

Only test important things. Follow `context/code-standards.md` → "Testing":
test behaviour users depend on against independent reference data; do not
add tests that restate the code, duplicate existing coverage, or exist to
raise a test count. Edge cases go into the fixture generator.

## Documentation and API Verification

Never write code against an external API, library, SDK,
or CLI tool from memory. Follow
`context/code-standards.md` → "Documentation and API
Verification" (check pinned version → package CLI →
Context7 via the `find-docs` skill → official docs →
halt and ask the user if none is available), and log
sources in `context/progress-tracker.md` → "API
Verification Log".
