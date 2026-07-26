# Pure-F407 Repository Rules

This is the active pure-F407 DJI C-board workspace. H7 is not part of the
product build or normal user surface.

- Shared framework/API/configuration-generation changes originate in
  `../pnx_h7_f4`; promote an accepted export rather than creating a
  candidate-only shared-module fork.
- Board-specific development and hardware validation happen here.
- Use `../OLDF4` and RM26 material for hardware facts, not as architecture
  authority.
- Keep `pnx_bsp`, `pnx_devices`, `pnx_libs`, and `pnx_modules` at their exact
  recorded gitlinks.
- Motor output defaults to zero. Non-zero output requires a separately
  approved, bounded bench gate.
- USB descriptor identity remains unassigned until explicitly approved;
  controller start must remain fail-closed.
- Do not push, add/change remotes, tag, publish, or operate hardware without
  explicit authorization.
- Separate verified facts, user constraints, assumptions, inferences, and
  unknowns.
- Hardware claims require retained logs, telemetry, captures, and the exact
  ELF hash.

Read [HANDOFF.md](HANDOFF.md), then
[docs/CURRENT_TASK.md](docs/CURRENT_TASK.md).
