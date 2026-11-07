# ADR 0001: Record Architecture Decisions

## Status
Accepted

## Context
We need a standard way to record architectural decisions made during the development of Azaan Tray, preserving the rationale, trade-offs, and consequences for contributors and future maintainers.

## Decision
We will use Architecture Decision Records (ADRs) structured in `docs/adr/`. Each ADR will record:
- Title and status (Proposed, Accepted, Superceded)
- Context and problem statement
- Decision taken
- Consequences and trade-offs

## Consequences
- Key architectural choices are documented explicitly in the repository alongside code.
- Decisions can be referenced from `context/progress-tracker.md` and commit messages.
