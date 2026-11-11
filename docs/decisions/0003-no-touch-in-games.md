# Touch is not available to games

- **Date:** 2026-11-11
- **Status:** decided

## Question
Board B has a touchscreen. May games use it?

## Options considered
- Expose touch on Board B only.
- Keep games on knobs and tilt on both boards.

## Decision
Games never receive touch input.

## Evidence
Every game must run on both boards, and two knobs plus tilt is the platform's identity. This was assumed by the spec author and not yet explicitly confirmed by the maintainer.

## Reopen if
The maintainer wants touch-only features, or Board B cannot be fitted with knobs.
