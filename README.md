# Ripple â€” Incremental Build Impact Analyzer

<p>
  <b>Change one file â†’ know exactly what to rebuild, in the right order, in milliseconds.</b><br>
  Powered by the <b>RIPPLE</b> algorithm â€” a dependency-graph engine that turns <i>"I edited a header"</i> into a minimal, ordered rebuild
  plan â€” the same way industrial build systems (Ninja, Bazel, MSBuild) stay fast.
</p>

```
== Ripple incremental analysis ==
scanned 9 file(s), graph edges 9

[~] changed (1)
  ~ user_model.h

[*] affected (3)
  * home.cpp
  * user_model.cpp
  * user_model.h

[>] build order (3)
   1. user_model.h
   2. home.cpp
   3. user_model.cpp
```

<p align="center">
  <img src="docs/images/then-original-idea.svg" width="400" alt="The original idea: manual dependencies, LAST_EDIT, affected files, partial topological order" />
  &nbsp;&nbsp;
  <img src="docs/images/now-ripple-engine.svg" width="400" alt="Now: the RIPPLE engine with its 5 stages" />
</p>

| | **then** (the original sketch) | **now** (the RIPPLE engine) |
|---|---|---|
| dependencies | hand-written `addDependency()` calls | auto-scanned from `#include "..."` â€” zero config |
| change detection | `LAST_EDIT` timestamp only | FNV-1a 64-bit of the **bytes** + mtime/size shortcut |
| new / renamed files | broke the map | picked up on the next run |
| affected set | whole reachable closure | same, O(V + E), minimal |
| build order | printed, unordered inside groups | deterministic Kahn order (alphabetical ties) |
| dependency cycles | silent / hung | detected and reported |
| state between runs | plain text, overwrite-in-place | version-tagged binary, atomic `tmp` + `rename` |
| interactivity | none | live TUI: arrow-key picker + what-if simulation |

---

## Why

Re-running a full build after one small edit wastes most of the time on files
that did not change and cannot be affected. Ripple answers the two questions
every incremental builder must answer:

1. **What actually changed?** â€” content fingerprints (FNV-1a 64-bit) with a
   `mtime+size` fast path, diffed against a persisted baseline.
2. **What is forced to rebuild, and in what order?** â€” a `#include`-derived
   dependency graph, BFS for impact, Kahn's algorithm for a deterministic
   compile/link order. Cycle detection warns when an order cannot exist.

With 500 files where 2 headers changed, the plan touches ~10 files, not 500.
That is the entire point: **speed up every run based on what the user edited**.

## The RIPPLE algorithm â€” the designed name

An edited file is a **stone dropped into water**; only the files it is
*connected to* feel the wave. Five named stages:

| # | stage | classic algorithm inside |
|---|-------------|------------------------------|
| 1 | **SCAN** â€” walk the folder, turn `#include "..."` into graph edges | directory walk + include parser |
| 2 | **FINGERPRINT** â€” hash bytes, compare with the last run | FNV-1a 64-bit + mtime/size filter |
| 3 | **RIPPLE** â€” spread out along "who includes me" edges | BFS (breadth-first search) |
| 4 | **ORDER** â€” dependency-first build sequence | Kahn's topological sort |
| 5 | **SAVE** â€” atomically persist the new baseline | write-tmp + rename |

The app ships with `(5|h) how it works` in the interactive menu â€” the same
table, live.

## Features

- **Zero configuration.** No hand-written dependency lists. The scanner walks
  the tree, reads every `#include "..."` and builds the graph dynamically â€”
  new files, renamed files and rewired headers are picked up on the next run.
- **mtime/size short-circuit.** Untouched files never get re-hashed.
- **Live state.** Baseline is rewritten atomically (`tmp` + `rename`) after
  every run; a crash never corrupts it.
- **Interactive terminal graph.** Arrow-key file picker and what-if trees,
  drawn with ANSI box art. A *simulation* â€” it never touches your sources.
- **Honest edge semantics.** `#include <x.h>` means *x.h changed â†’ includer
  rebuilds*. Mirrors what real compilers need, so reports are not guesses.

## Quick start

```sh
# build (pick one)
cmake -B build -S . && cmake --build build      # CMake (any platform)
scripts\build.bat                               # MSVC, no CMake needed
./scripts/build.sh                              # GCC/Clang

# demo project included
build/ripple init samples/demo_app     # capture baseline
# ...edit any file under samples/demo_app...
build/ripple status samples/demo_app   # changed / affected / build order
build/ripple what-if samples/demo_app user_model.h
build/ripple scan samples/demo_app     # raw edge list
build/ripple                           # interactive menu
```

Ripple is **not** a build tool; it does not invoke your compiler. It produces
the plan a build tool (or a plain `for` loop) executes.

## Commands

| command | effect |
|---------|--------|
| `ripple init [dir]` | capture baseline state (`[dir]/.ripple/baseline.bin`) |
| `ripple status [dir]` | diff vs baseline â†’ changed / added / removed / affected / build order |
| `ripple what-if [dir] [file]` | simulated impact of editing a file (interactive picker when omitted) |
| `ripple scan [dir]` | discovery summary + raw adjacency (debug view) |
| `ripple` | interactive terminal menu |

## How it works (pipeline)

```mermaid
flowchart TD
    A[source tree] --> B[SourceScanner]
    B -- '#include "..." edges' --> C[DependencyGraph]
    A -- 'fs.stat' --> D[mtime+size filter]
    C --> E[BFS over dependents]
    D --> F{mtime+size equal?}
    F -- yes --> G[hash reused]
    F -- no --> H[FNV-1a 64-bit hash]
    D --> P{diff vs baseline}
    H --> P
    G --> P
    P -- changed --> E
    E --> KA[Kahn topological order]
    KA --> R[minimal rebuild plan]
    P --> S[baseline.bin atomic update]
```

Core reality check, one run on this repo's demo:

| stage | cost |
|-------|-------|
| directory walk + include parse | O(files Ã— lines) |
| unchanged files | O(1) each (stat only) |
| changed files | O(bytes) hash |
| impact BFS + Kahn | O(V + E) |

## Interactive what-if

```
$ ripple what-if samples/demo_app user_model.h

what-if: changing 'user_model.h' requires rebuilding 3 file(s)
order: user_model.h -> home.cpp -> user_model.cpp
user_model.h  <= changed
â”œâ”€â”€ home.cpp *affected*
â””â”€â”€ user_model.cpp *affected*
```

Menu mode lets you walk the whole tree the same way with arrow keys â€”
the graph lives in your terminal, and nothing on disk changes.

## Project layout

```
include/ripple/     public headers (graph, scanner, analyzer, tui, ...)
src/               implementation + CLI entry point
samples/demo_app/  tiny 9-file project wired as a demo graph
docs/              ALGORITHM.md (deep dive), GRAPH.md (visual guide)
scripts/           build.bat (MSVC), build.sh (GCC/Clang)
```

## Design notes

- **Locale-independent case folding** on include names (byte-wise ASCII only).
- **Byte-normalized keys** â€” backslashes become slashes on disk and in the
  baseline, so Windows/Linux runs share state.
- **Dynamic, not static.** Nothing about the project is compiled in; every run
  re-derives everything from the directory it is pointed at.
- **Reaction to "weird" states**: `#include` of a name that exists on disk but
  with an unknown extension is still resolved to a node; cycles print a
  warning instead of lying about an order.

## Roadmap

- watcher mode (`ripple watch` â€” react to file system events)
- parallel-hash pass over changed files
- `.d` file export (drop `gcc -MF` equivalents into the plan)

## License

MIT â€” see [LICENSE](LICENSE).
