# Engine Discovery Reference

How `typio` locates and loads engine plugins at startup.

## Search path (priority order)

| Order | Source | Path |
|---|---|---|
| 1 | `-E` / `--engine-dir DIR` | directories given on the command line; **repeatable**, scanned in the order given |
| 2 | `$TYPIO_ENGINE_PATH` | colon-separated list, scanned in listed order |
| 3 | System lib dir | compile-time `<prefix>/<libdir>/typio/engines` (e.g. `/usr/lib/typio/engines`, or `/usr/local/lib/typio/engines` for a `/usr/local` prefix) |

- All existing directories in the list are scanned, in the order above.
- The **first** engine of a given `<name>` registers; a later duplicate is rejected (`AlreadyExists`).
- The daemon auto-loads only from the system directory (order 3). Orders 1 and 2 are explicit operator opt-ins; there is no per-user `$HOME` auto-scan. See [ADR-0025](../adr/0025-engine-discovery-search-path.md) for the rationale (engines are keystroke-privileged native code).

## File name convention

| Rule | Value |
|---|---|
| Required prefix | `libtypio_engine_` |
| Required suffix | `.so` |
| Engine identifier | `<name>` — the text between prefix and suffix |
| Loaded example | `libtypio_engine_basic.so` → identifier `basic` |
| Ignored | any file not matching `libtypio_engine_*.so` (silently skipped) |

- Cargo emits `libtypio_engine_<name>.so` natively — no rename needed.
- `<name>` is the identifier used in config keys (`engines.<name>.*`), the `--engine` flag, and `typioctl`.

## Bundled icons (optional)

| Item | Value |
|---|---|
| Location | `<engine-dir>/icons/` (freedesktop hicolor layout) |
| Effect | the directory is added to the tray's `IconThemePath` |
| Resolves | `TypioEngineInfo.icon` and the engine's status `icon_name` |

## Related

- [How to Package for Distribution](../how-to/package-for-distribution.md) — install paths for packagers
- [Troubleshooting: no engines](../how-to/troubleshooting.md) — common discovery failures
- [Developer Setup](../dev/setup.md#engine-discovery) — building an engine and pointing the daemon at it locally
