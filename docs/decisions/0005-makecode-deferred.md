# MakeCode and block-based editing are deferred

- **Date:** 2026-11-11
- **Status:** decided

## Question
Should v2 include MakeCode import or a block editor?

## Options considered
- Build an importer now.
- Build a block editor now.
- Defer both.

## Decision
Deferred. Nothing is built for either in v2.

## Evidence
Maintainer decision. A faithful converter would mean reimplementing another runtime, and a block editor only becomes practical once games are loadable files.

## Reopen if
Loadable games are shipped and the maintainer asks for it.
