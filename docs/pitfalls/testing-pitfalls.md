# Testing Pitfalls

This file collects testing traps for the Spronk Quest GBA project to help write reliable host-side unit tests.

- **TEST-1:** Logic tests must not depend on frame timing; integrate physics with an explicit fixed `dt`/tick.
- **TEST-2:** Fixed-point tests must assert exact raw integer values, not float-compared approximations.
- **TEST-3:** Collision tests must cover the tunneling case (high velocity through a thin tile).
- **TEST-4:** Save tests must cover corrupted/empty SRAM (first boot).
