# The stage is 10 blocks tall and at least 17 wide

- **Date:** 2026-11-11
- **Status:** decided

## Question
How do games stay consistent on a 320x172 and a 280x240 screen?

## Options considered
- Design for the smaller area and letterbox.
- A fixed stage height with the extra space used differently per board.

## Decision
The stage is 10 blocks tall and at least 17 blocks wide. Board A shows 20 columns with a thin status strip; Board B shows 17.5 columns with a status panel, or 15 rows in fill view.

## Evidence
Arithmetic at 16 screen pixels per block: 320x172 is 20 x 10.75 blocks, 280x240 is 17.5 x 15. Both screens are about 33 mm wide. The block size itself is still provisional until the test-card gate (spec M1).

## Reopen if
The test-card gate picks a block size other than 16 screen pixels; redo the arithmetic.
