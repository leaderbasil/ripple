# ALGO 1 — Incremental Build Impact Analyzer

<p>
  <b>Change one file → know exactly what to rebuild, in the right order, in milliseconds.</b><br>
  A dependency-graph engine that turns <i>"I edited a header"</i> into a minimal, ordered rebuild
  plan — the same way industrial build systems (Ninja, Bazel, MSBuild) stay fast.
</p>

```
== Algo1 incremental analysis ==
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

---

## Why

Re-running a full build after one small edit wastes most of the time on files
that did not change and cannot be affected. ALGO 1 answers the two questions
every incremental builder must answer:

1. **What actually changed?** — content fingerprints (FNV-1a 64-bit) with a
   `mtime+size` fast path, diffed against a persisted baseline.
2. **What is forced to rebuild, and in what order?** — a `#include`-derived
   dependency graph, BFS for impact, Kahn's algorithm for a deterministic
   compile/link order. Cycle detection warns when an order cannot exist.

With 500 files where 2 headers changed, the plan touches ~10 files, not 500.
That is the entire point: **speed up every run based on what the user edited**.

## Features

- **Zero configuration.** No hand-written dependency lists. The scanner walks
  the tree, reads every `#include "..."` and builds the graph dynamically —
  new files, renamed files and rewired headers are picked up on the next run.
- **mtime/size short-circuit.** Untouched files never get re-hashed.
- **Live state.** Baseline is rewritten atomically (`tmp` + `rename`) after
  every run; a crash never corrupts it.
- **Interactive terminal graph.** Arrow-key file picker and what-if trees,
  drawn with ANSI box art. A *simulation* — it never touches your sources.
- **Honest edge semantics.** `#include <x.h>` means *x.h changed → includer
  rebuilds*. Mirrors what real compilers need, so reports are not guesses.

## Quick start

```sh
# build (pick one)
cmake -B build -S . && cmake --build build      # CMake (any platform)
scripts\build.bat                               # MSVC, no CMake needed
./scripts/build.sh                              # GCC/Clang

# demo project included
build/algo1 init samples/demo_app     # capture baseline
# ...edit any file under samples/demo_app...
build/algo1 status samples/demo_app   # changed / affected / build order
build/algo1 what-if samples/demo_app user_model.h
build/algo1 scan samples/demo_app     # raw edge list
build/algo1                           # interactive menu
```

ALGO 1 is **not** a build tool; it does not invoke your compiler. It produces
the plan a build tool (or a plain `for` loop) executes.

## Commands

| command | effect |
|---------|--------|
| `algo1 init [dir]` | capture baseline state (`[dir]/.algo1/baseline.bin`) |
| `algo1 status [dir]` | diff vs baseline → changed / added / removed / affected / build order |
| `algo1 what-if [dir] [file]` | simulated impact of editing a file (interactive picker when omitted) |
| `algo1 scan [dir]` | discovery summary + raw adjacency (debug view) |
| `algo1` | interactive terminal menu |

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
| directory walk + include parse | O(files × lines) |
| unchanged files | O(1) each (stat only) |
| changed files | O(bytes) hash |
| impact BFS + Kahn | O(V + E) |

## Interactive what-if

```
$ algo1 what-if samples/demo_app user_model.h

what-if: changing 'user_model.h' requires rebuilding 3 file(s)
order: user_model.h -> home.cpp -> user_model.cpp
user_model.h  <= changed
├── home.cpp *affected*
└── user_model.cpp *affected*
```

Menu mode lets you walk the whole tree the same way with arrow keys —
the graph lives in your terminal, and nothing on disk changes.

## Project layout

```
include/algo1/     public headers (graph, scanner, analyzer, tui, ...)
src/               implementation + CLI entry point
samples/demo_app/  tiny 9-file project wired as a demo graph
docs/              ALGORITHM.md (deep dive), GRAPH.md (visual guide)
scripts/           build.bat (MSVC), build.sh (GCC/Clang)
```

## Design notes

- **Locale-independent case folding** on include names (byte-wise ASCII only).
- **Byte-normalized keys** — backslashes become slashes on disk and in the
  baseline, so Windows/Linux runs share state.
- **Dynamic, not static.** Nothing about the project is compiled in; every run
  re-derives everything from the directory it is pointed at.
- **Reaction to "weird" states**: `#include` of a name that exists on disk but
  with an unknown extension is still resolved to a node; cycles print a
  warning instead of lying about an order.

## Roadmap

- watcher mode (`algo1 watch` — react to file system events)
- parallel-hash pass over changed files
- `.d` file export (drop `gcc -MF` equivalents into the plan)

## License

MIT — see [LICENSE](LICENSE).
