# YATL

**Yet Another TOML Library** — A format-preserving TOML parser and editor written in C.

## Overview

YATL is designed with simplicity and reliability in mind:

- **Minimal allocations** — Read operations aim for zero allocations; write operations use stack memory primarily with minimal heap usage for line management
- **Format-preserving** — Edits maintain your original formatting, comments, and whitespace
- **Simple API** — Clean, straightforward syntax for parsing and editing
- **Memory-safe by design** — Heavy use of stack and static memory, opaque types, and initialization detection to prevent common memory bugs

## Documentation

See [YATL.md](YATL.md) for detailed usage information and the API documentation at https://dylanetaft.github.io/YATL/

## Examples

Check out the [examples](examples/) directory for working code samples.
