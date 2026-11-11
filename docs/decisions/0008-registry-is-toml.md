# The registry and game settings are TOML, and the starter key is `starter`

- **Date:** 2026-11-11
- **Status:** decided by the implementer; flagged to the maintainer for review

## Question
The spec named the parts registry `engine/parts.yaml`, and its `game.toml` example used a top-level `kind = "platformer"` next to `[kind.<name>]` tables.

## Options considered
- YAML registry, which needs a third-party Python package to read.
- TOML registry, read by Python's standard library (3.11+).

## Decision
- The registry is `engine/parts.toml`.
- The starter is named by `starter = "platformer"`. A TOML file cannot have `kind` as both a value and a table, so the spec's example could not be parsed.
- `game.toml` also carries `id`, which must equal the folder name.

Spec sections 4, 9, 10 and 17 are updated.

## Evidence
Python has `tomllib` built in and no YAML reader; the tools must run on a family computer without installing packages. The `kind` clash is a TOML rule.

## Reopen if
The registry needs structure TOML cannot express.
