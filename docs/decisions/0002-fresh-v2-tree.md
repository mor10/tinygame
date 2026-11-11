# Rebuild as a fresh v2 tree

- **Date:** 2026-11-11
- **Status:** decided

## Question
Update v1 in place, or rebuild?

## Options considered
- Incremental updates on main.
- A fresh v2 tree in the same repository, carrying over proven modules.

## Decision
Fresh v2 tree. v1 is tagged v1.0. Hardware-verified drivers, the pure state machines with their tests, the dev protocol and the simulator plumbing are carried over; the list is in spec section 4.

## Evidence
About 7,500 of v1's 16,500 lines are the three games, which are rewritten anyway. The game API, main loop, display path, test runner and agent guidance all change. There is no installed base.

## Reopen if
Carrying modules over proves harder than rewriting them.
