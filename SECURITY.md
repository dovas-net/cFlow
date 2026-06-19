# Security Policy

`flow` is a single-header C library for terminal node-graph editors. It is a
**library**: it runs inside a host program, so the embedder owns the threat model.
This document states what `flow` defends against, what it assumes the caller has
already vetted, and how to report a vulnerability.

## Supported versions

`flow` is pre-1.0; the public API may change between minor versions until it locks
at `1.0.0` (see [`CHANGELOG.md`](CHANGELOG.md)). Security fixes target the **latest
released version** only. Distribution is a single copied `flow.h`, so updating means
replacing that file with the patched release.

| Version | Supported |
| ------- | --------- |
| latest `0.x` release | yes |
| older `0.x` | no — update to the latest release |

## Reporting a vulnerability

Use GitHub's **private vulnerability reporting**: go to the repository's **Security**
tab and choose **"Report a vulnerability"**. This opens a private advisory visible
only to the maintainers.

- **Do not open a public issue or pull request for a suspected vulnerability.** Report
  it privately so a fix can ship before details are public.
- Maintainers: if the **"Report a vulnerability"** button is absent, enable *Private
  Vulnerability Reporting* under the repository's Security settings.

Please include the affected version, a minimal reproducer (ideally a `flow_load`
input or a `flow_feed` byte sequence), and the observed effect.

## Security model and attack surface

`flow` is split into portable pure-C layers (model, geometry, cell buffer, render,
routing, layout, JSON) and a POSIX/terminal-bound run-loop (`flow_run` /
`flow_present` / `flow_term_*`). The library is **not thread-safe**: serialize calls
per `flow_t`. See [`README.md`](README.md),
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) (the portable-core / POSIX-terminal
boundary), and [`docs/API.md`](docs/API.md) for the layer boundary and per-function
detail.

### Primary untrusted-input surface: `flow_load`

`flow_load` (`src/flow_json.h:291`) is the one entry point that parses bytes you did
not necessarily produce. **Treat any `flow_load` input you did not generate yourself
as untrusted** — a saved-graph file is attacker-controllable data.

Hardening already in place in the JSON reader:

- **Validate before mutate.** `flow_load` reads the whole file, runs a structural
  validity pass (balanced, string-aware `{}`/`[]` nesting; no unterminated strings)
  **before** it resets the live graph. A **structurally** malformed or truncated file
  (unbalanced `{}`/`[]`, unterminated string) returns `-1` with the existing graph
  untouched. Note: this pre-reset pass is **structural only** — a structurally-valid
  file with semantically-invalid contents passes validation and resets the graph
  before rebuilding best-effort.
- **Bounded parsing.** The hand-rolled reader is no-DOM and bounds every step by the
  buffer end; string values are unescaped into fixed buffers with clamping.
- **No-leak, signaled allocation failure.** Out-of-memory is signaled, not fatal:
  `flow_new` returns `NULL` and `flow_add_node` / `flow_add_edge` return `-1`
  (see the `flow.h` header preamble and the `0.2.0` entry in
  [`CHANGELOG.md`](CHANGELOG.md)). The growth primitive does not leak the old block
  when `realloc` fails — the caller keeps its previous allocation
  (`src/flow_model.h:481`).

**Embedder responsibility — custom type hooks.** When a node type registers a `load`
hook, `flow_load` hands it a NUL-terminated copy of the attacker-controlled `data`
span (`src/flow_json.h`). The library's bounds-safety stops at that hand-off: a hook
that parses the span must validate its own input.

### Interactive input

The interactive run-loop's input feed (`flow_run` reading a local terminal via
`flow_feed`) consumes **local terminal keystrokes/mouse**, which are trusted as
local user input. This trust holds for the **local interactive** path only. In the
embed path, `flow_feed` accepts bytes from whatever source you wire in — including a
socket or pty (see [`README.md`](README.md)). If that source is not local, treat
those bytes as untrusted as well.

### Terminal handling

On the POSIX path `flow` installs signal and crash handlers that restore the terminal
(raw mode, alt-screen, cursor, mouse tracking) and chain to the prior handler; they
are removed on `flow_term_restore` and never installed on the headless path. If you
put the terminal in raw mode yourself, restore it yourself. See the **Terminal
safety** section of [`README.md`](README.md).

### Planned hardening

Input-range validation hardening for `flow_load` and a fuzz harness for the JSON
reader are planned before `1.0`. Until then, the guidance above stands: treat
`flow_load` input as untrusted and validate at the boundary you control.
