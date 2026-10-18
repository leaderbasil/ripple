# ALGORITHM.md - Deep dive into the RIPPLE algorithm

**RIPPLE** is the designed name of this engine: an edited file is a stone
dropped in water, and only the files *connected to it* feel the wave.
Five stages: **S**can -> **F**ingerprint -> **R**ipple (BFS) -> **O**rder (Kahn)
-> **S**ave (atomic baseline). Below is why each stage is that algorithm
and nothing simpler.

This document explains every stage of the pipeline: what runs, why it
is correct, and what it costs. The sibling document
[`GRAPH.md`](GRAPH.md) is the visual/walkthrough version.

## Problem statement

Given a source tree `T` and a user edit to one file `u in T`, compute:

1. `changed` - the set of files whose *bytes* differ from the last run,
2. `affected` - the smallest set of files that must be rebuilt,
3. `order` - a total order on `affected` in which dependencies build before
   dependents.

Everything runs in **one process** per invocation, no compiler is launched,
and the state is small enough to live next to the sources.

## Stage 1 - Dynamic discovery (`SourceScanner::scan`)

```
for each regular file under root (recursively):
    skip any path containing a component starting with '.'
      (.git, .ripple, .vs)  -- state must not poison itself
    normalize to forward slashes
sort the relative paths
```

*Cost*: O(tree size). Output is sorted so every later stage sees a stable,
reproducible stream - this is what makes byte-diffs and deterministic Kahn
tie-breaking possible.

Then, per file, every line is checked against a tiny parser for quoted
include directives:

```
optional whitespace '#include' whitespace? '"' <target> '"'
```

* System includes `<...>` are ignored (they are not project files).
* Self-includes are ignored (a file cannot be its own dependency).
* Case folding is byte-wise ASCII (`tolower` on `unsigned char`) - correct
  for source file names on case-insensitive filesystems, no locale surprises.
* Bare names (`"user_model.h"`) resolve as if relative to any scanned file;
  qualified names (`"sub/thing.h"`) resolve relative to the root.
* If nothing matches, the include lands in `unresolved` and is reported, not
  silently dropped. Unresolved edges cannot silently corrupt the graph.

**Key property**: nothing about the project is baked into the binary. Every
run re-derives the graph from disk. Add a file, delete a file, rewire a
header - the next run simply agrees with reality.

## Stage 2 - The graph (`DependencyGraph`)

Edges are stored **both directions** (each O(1) appends, deduplicated):

```
dependents_[a]    = files b such that b includes a   // forward (impact)
dependencies_[b]  = files a such that b includes a   // reverse (order)
```

Semantics are the single most important design decision: an edge
`a -> b` always means *"if a's bytes change, b may need a rebuild"*. With
that one convention, `affectedBy` and `buildOrder` are straight-line graph
algorithms and no per-file heuristics creep in.

Isolated files (a `.cpp` nobody includes and that includes nothing local)
are registered as orphan nodes, so they stay visible in reports and in the
baseline rather than silently vanishing.

## Stage 3 - Change detection (`Analyzer::run`)

For every file, in scan order:

```
stat(mtime, size)  (one syscall)
if stat == recorded -> re-use previous hash, skip read
else                -> stream-hash the bytes (FNV-1a 64-bit, 64 KiB chunks)
```

Diff against the persisted baseline:

* `changed` - in baseline, in tree, hash differs
* `added` - in tree, not in baseline
* `removed` - in baseline, not in tree

The identity function is **content**, not timestamps: `mtime+size` only acts
as a cheap pre-filter. A file that keeps the same mtime and size but changes
bytes (say, a one-character swap) gets hashed and caught. This is the
practical sweet spot between "every build distrusts everything" and the
sloppier "trust anyone who touches the timestamp".

`FNV-1a 64-bit` is chosen deliberately:

* one pass over the input, ~1 GB/s - no openssl, no CRC table, no architecture
  dependencies,
* deterministic across runs, machines and processes, so the baseline binary
  remains valid forever,
* 64-bit space: for realistic source-code change patterns, the risk of
  collision *plus artifact detection failing simultaneously* is negligible;
  the mtime/size drift that accompanies real edits makes a false negative
  require an adversary.

## Stage 4 - Impact (`affectedBy`) - the actual RIPPLE

```
work = changed (dedup, only nodes present in the graph)
while work not empty:
    n = pop
    for d in dependents_[n]: if d unseen -> mark + push
```

Depth-first with a visited set. **Cost**: O(V + E) - every node and edge is
considered at most once. The result contains the changed files themselves
plus everything transitively downstream of them.

Correctness: if `b` includes `a`, `b` cannot compile new code without
re-reading `a`'s declarations; transitively, every file reachable from a
changed file over `dependents` edges is exactly the classic "recompile if the
text you include was touched" rule.

## Stage 5 - Order (`buildOrder`) - Kahn's algorithm

```
indegree[n] = |dependencies within affected|
ready  = min-heap of nodes with indegree == 0 (key = file name)
while ready not empty:
    n = pop smallest
    emit n
    for d in dependents_[n] within affected:
        --indegree[d]; when 0, push d
leftover = nodes never emitted
```

Why Kahn's and not DFS topological sort: the emitted prefix here is
**maximal and usable even when cycles exist** - DFS sort gives no such
guarantee. Anything stuck in a cycle is reported in `cyclic` with a warning
instead of inventing an order that links a broken artifact.

Deterministic: heap-ordered by name, so two runs on the same state produce
byte-identical output (CI diffable, tests snapshotable).

*Cost*: O(V log V + E) with the heap; O(V + E) in decisions.

## Stage 6 - Persistence (`Baseline::save`)

After every run the current `file -> (hash, mtime, size)` table is written to
a version-tagged binary next to the sources, using **write-tmp-then-rename**.
An interrupted write leaves the previous file untouched; a corrupted or
ancient file is detected by magic/version bytes and treated as a fresh
baseline rather than a lie.

## Complexity summary

| stage | cost | note |
|-------|------|------|
| discovery + include parse | O(S) | S = tree text size |
| unchanged-file check | O(1) each | stat short-circuit |
| changed-file hashing | O(bytes) | chunked streaming |
| impact BFS | O(V + E) | |
| Kahn order | O(V log V + E) | heap tie-breaking |
| baseline save | O(V) | atomic rename |

For a warm run over a 10 000-file tree with 3 edited headers: 9 998 stats + 3
hashes + graph math - dominated by the OS, not this tool. That is exactly the
"speed up every run based on what the user edited" goal.

## What it deliberately does not do

* No compiler invocation, no parallel scheduling, no workspace-abstraction.
  A scheduling layer is a different problem with different inputs (build
  resources, cache hits); Ripple feeds it a correct, minimal, ordered input.
* No partial preprocessor semantics: `"#include"` inside a string or comment
  could theoretically be matched. The parser keeps to literal, first-block
  `#include` syntax; false positives would only ever add a *superset* of a
  correct affected set (never omit work), so safety is preserved.
