# C legibility standard — scope ruling + vendored text

Owner ruling (Chase, 2026-08-26). Vendored from 7etsuo/write-legible-c
(github.com/7etsuo/write-legible-c @ `2f624ed0298702354dffd9aa9900df24ce286ed2`,
MIT, Copyright (c) 2026 7etsuo — NOTICES entry "write-legible-c"). Everything
below the `VENDORED TEXT` marker is that repo's
`plugins/write-legible-c/skills/write-legible-c/references/c-standard.md`
byte-verbatim, sha256
`9932bfc9fa4e8efa290d6cd6e6b9628486e293e6dc6a10fc1d136d035fb0e237`.
Never edit below the marker; deltas live here.

## Scope — two planes, one rule each

- **Translation TUs — the standard does NOT apply.** Anything mirroring
  upstream JS body-for-body (`port/sim/characters/`, `physics.c`,
  `hit_detection.c`, `article.c`, `interpolated_collision.c`,
  `action_state_shortcuts.c`, `input/`, `stages/`, `ai.c`, and any future
  translation): HARD RULE 5 faithfulness wins, full stop. Upstream typos,
  dead arms, pasted blocks, and control-flow shape are carried verbatim —
  "refactoring" them destroys the structure-parallel diffability the whole
  calibration method depends on.
- **Original TUs — applies FORWARD-ONLY.** Code with no upstream to mirror
  (`port/foh/`, `port/gfx/`, `port/sim/target/`, `port/sim/device/` tools,
  the future M4 mixer): new files and newly-written functions follow the
  standard; touched code may be brought along when the diff is already
  there. NO retrofit sweeps — churn without behavioral payoff.

## Deltas from the vendored text (this repo's rulings)

- **§13 build flags:** our pinned set is `-O2 -ffp-contract=off -Wall
  -Wextra -Werror` (frozen inside check scripts). Do NOT add `-Wconversion`
  / `-Wshadow` to existing TUs; a new standalone tool may opt in.
- **§4 40-line cap:** softened for draw functions that are honest tables of
  drawing calls (gfx/foh); the cap holds for logic.
- **§9 MODULE_TRY:** adopt only if a module organically grows the pattern;
  do not invent per-module TRY macros speculatively.
- **§17 AGENTS.md:** already covered (CLAUDE.md / docs/); do not add one.
- Adopted as-is and worth internalizing: module prefixes, per-module status
  enums, contract comments, no naked literals, bounded loops / no
  recursion, boundary-validate-then-assert (§8), the altitude model + name
  test (§4), the §14 pre-delivery checklist, the §16 near-miss diagnosis.

## Review wiring

Reviewer prompts for diffs touching original-plane C must include the
§14 + §16 lens — see PROCESS.md §3.2.

---

# ===================== VENDORED TEXT (do not edit) =====================

# C standard for machine-written code

Rules for C that is maximally legible to language models and humans. Every
rule exists to cut the tokens and working memory needed to reason about any
region of code, to make every symbol greppable to few sites, and to keep every
edit local. Follow the rules mechanically. Deviation requires a comment at the
deviation site explaining why.

## Contents

1. [File layout](#1-file-layout)
2. [Constants](#2-constants)
3. [Naming](#3-naming)
4. [Functions](#4-functions)
5. [Control flow](#5-control-flow)
6. [Errors](#6-errors)
7. [Types and data](#7-types-and-data)
8. [Boundary discipline](#8-boundary-discipline)
9. [Macros](#9-macros)
10. [Duplication](#10-duplication)
11. [Comments](#11-comments)
12. [Formatting mechanics](#12-formatting-mechanics)
13. [Build hygiene](#13-build-hygiene)
14. [Pre-delivery checklist](#14-pre-delivery-checklist)
15. [Skeleton](#15-skeleton)
16. [Worked example: good but not good enough](#16-worked-example-good-but-not-good-enough)
17. [The repository level](#17-the-repository-level)
18. [Provenance](#18-provenance)

## 1. File layout

Every `.c` file has this exact order:

1. File comment: one or two lines stating what this module owns and does.
2. Includes: system headers, blank line, project headers. Alphabetical within
   each group.
3. Constants: enums first, then defines.
4. Types: structs, unions, typedefs.
5. Prototypes for every static function, each with its contract comment.
6. Public function definitions, in the same order as the header.
7. Static function definitions, in call order.

Every `.h` file: include guard, includes, constants, types, prototypes.
Nothing else. No function bodies, no variables.

Why: a model reading the first screen of the file learns the complete
vocabulary before meeting any logic. It never encounters an unexplained
symbol, so it never spends tokens searching downward to resolve one.

## 2. Constants

- No naked literals. The only bare numbers allowed in logic are 0 and 1 where
  meaning is obvious. Every other value gets a name at the top of the file or
  in the header.
- Use `enum` for related integer constants so debuggers and models see names,
  not values. Use `#define` only for string literals and conditional
  compilation.
- Units go in the name: `TIMEOUT_MS`, `MAX_PAYLOAD_BYTES`, `RETRY_COUNT`.
- Derived values are computed, never restated:
  `POOL_BYTES = POOL_SLOTS * SLOT_BYTES`.
- One definition site per constant in the whole program.

Why: a magic number is a fact with no grep anchor. A named constant is
self-documenting at every use site and editable in exactly one place.

## 3. Naming

- Every symbol with external linkage carries the module prefix: `rb_push`,
  `net_send`. Static helpers carry it too.
- Functions are verb_object: `parse_header`, `flush_queue`. Predicates start
  with `is_` or `has_` and are never negated: `is_valid`, never
  `is_not_ready`.
- Lifetime pairs are exact: `_create` pairs with `_destroy` and implies heap
  allocation with ownership transfer. `_init` pairs with `_deinit` and
  implies caller-owned storage. `_open` pairs with `_close`.
- Sanctioned abbreviations: `buf`, `len`, `ctx`, `cfg`, `idx`. Nothing else.
  `tmp`, `data2`, `do_stuff` are banned.
- Variable name length scales with distance between declaration and last use.
  `i` is fine inside a 5-line loop. Anything crossing 20 lines gets a
  descriptive name.
- Precise beats verbose. A name is the shortest string that states the
  concept: `retry_count`, not `number_of_connection_retry_attempts`.
  Controlled studies of LLM code generation find concise precise identifiers
  outperform long composite names.

Why: consistent naming turns grep into a reliable navigation tool. A model
can find every producer of a symbol without reading intervening code. Naming
is also a semantic channel, not decoration: obfuscation studies show model
performance drops when names are stripped, even on execution tasks that
should depend only on structure, and human studies attribute up to a 30
percent comprehension effect to good names.

## 4. Functions

- One job per function. If the contract comment needs the word "and", split
  the function.
- Target 15 lines. Hard cap 40. A function is a table of contents for its
  helpers, not a container for logic phases.
- Maximum nesting depth 2. Guard clauses run first, then the happy path
  proceeds at the left margin.
- Parameter order is fixed: the module context pointer first, output
  parameters next, pure inputs last. A function with no context starts with
  its outputs, mirroring `memcpy`. A buffer and its length are always
  adjacent, buffer first.
- Maximum 4 parameters. More means the parameters are a struct trying to
  exist.
- No static variables inside functions except `static const` lookup tables.
  All state is passed in or owned by a context struct.
- Every function is an orchestrator, a leaf, or an adapter, never a mix. An
  orchestrator is a sequence of helper calls with status checks, branches on
  named predicates, and result binding, and contains no other logic. A leaf
  is straight-line logic that calls nothing but the module's accessors and
  pure utilities. An adapter wraps exactly one call into a foreign module and
  translates its status and calling convention into this module's.
- Decomposition terminates at the name test: if the most honest name for a
  candidate helper merely paraphrases its body, inline it and stop. A helper
  earns existence by naming a concept, owning an error value, or isolating a
  side effect.
- Cognitive complexity budget: target 8 per function, hard cap 15, measured
  by the Sonar rules that charge each break in linear flow and charge nesting
  progressively. Code conforming to the depth cap and decomposition rules
  sits far under budget; the metric is the tripwire, not the goal.

Why: a small flat function fits entirely in attention while reasoning about
any line of it. Nesting is state the reader must carry. Guard clauses
discharge preconditions immediately so nothing accumulates. The orchestrator
and leaf split gives every function one altitude: the reader follows a plan or
follows arithmetic, never both at once. The name test stops the regress toward
ravioli code, where a hundred two-line functions turn every read into a
pointer chase.

## 5. Control flow

- Early return over else chains. The success path is the unindented path.
- No `goto`. A function that acquires multiple resources decomposes into
  helpers where each helper releases what it acquired on its own failure,
  locally, next to the acquisition.
- A `goto` whose label only returns is banned with no exception. That is the
  cleanup pattern with the cleanup deleted: indirection purchasing nothing.
  Pure status propagation uses `MODULE_TRY`, section 9.
- Narrow exception: a function juggling three or more interdependent
  resources, where decomposition would smear half-initialized state across
  helpers, may use one forward `goto` to one cleanup label. The label site
  carries a comment justifying it.
- Every `switch` case ends in `break` or `/* fallthrough */`. `default` is
  always present.
- A loop body over 10 lines becomes a named function. The call site then
  documents the iteration in one line.
- Every loop has a statically evident upper bound. The one exception is an
  intentionally nonterminating loop, an event pump or scheduler, marked with
  a comment saying exactly that.
- No recursion, direct or indirect. Recursive shapes convert to a loop over
  an explicit bounded worklist, which makes stack use visible and termination
  checkable.
- No side effects inside conditions. No assignment inside `if`. Ternaries
  only for simple value selection, never nested.

Why: control flow is the most expensive thing to simulate while reading.
Every rule here reduces the number of paths a reader must hold to understand
one path.

## 6. Errors

- Every fallible function returns a module status enum. Success is 0 and is
  named: `RB_OK`.
- One status enum per module, values prefixed: `RB_ERR_ALLOC`, `RB_ERR_FULL`.
- Never return `bool` from anything that can fail more than one way. Never mix
  errno-style and enum-style in module code. Wrap libc at the boundary and
  convert.
- Every fallible call is checked. Status propagates upward unchanged. Only
  the top of the call chain logs, converts, or decides.
- Minimize producers per error value. `grep RB_ERR_FULL` should land on one
  producing line and its handlers.

Why: error enums are documentation, grep anchors, and debugging breadcrumbs
in one mechanism. A model tracing a failure follows the name straight to the
cause.

## 7. Types and data

- Fixed-width types everywhere: `uint32_t`, `int64_t`. `size_t` for sizes,
  counts, and indices. Bare `int` only where an external API forces it.
- `const` on every pointer parameter the function does not write through.
- Struct fields are grouped by relatedness, and every invariant that ties two
  fields together is documented in a comment above the struct.
- Opaque structs only at true library boundaries. Internal modules expose
  layout so instances can live on the stack, embed in parent structs, and be
  inlined across.
- Every union carries a tag field. Every variable is initialized at
  declaration. Structs use designated initializers.
- Declare every object at the smallest scope that works, at the latest point
  that works. Scope is working memory: the fewer live names at any line, the
  less state any reader carries.
- One level of dereference per expression. A chain like `a->b->c->d` smuggles
  three lifetimes and three nullability questions into one term; bind
  intermediates to named locals.
- Function pointers appear only as entries in `static const` dispatch tables.
  Control flow through data is legible when the table is immutable, named,
  and complete; a function pointer passed around loose is control flow no
  reader can trace.

## 8. Boundary discipline

- Public entry points validate their arguments and return `ERR_ARG` on
  violation.
- Internal static helpers do not re-validate. They `assert` their invariants.
  The assert documents the contract in debug builds and costs nothing in
  release.
- Assertion density is a feature. Every leaf that mutates state asserts at
  least one invariant, and the module-wide target is the Power of 10 floor of
  two assertions per function. Each assert is a machine-checked comment: it
  states what must stay true, at zero release cost, exactly where an editor is
  about to change something.

Why: validation logic duplicated at every level is noise that hides logic.
One validation site per boundary, asserts everywhere else, tells the reader
exactly who is responsible for what.

## 9. Macros

- Uppercase names. Every argument and the whole body parenthesized.
  Multi-statement bodies wrapped in `do { } while (0)`.
- No macro contains `return`, `goto`, `break`, or `continue`. No macro
  evaluates an argument twice.
- One sanctioned exception: each module may define `MODULE_TRY(expr)` beside
  its status enum, the sole macro permitted to contain a return. It evaluates
  `expr` once and returns any non-OK status to the caller. It is C's answer to
  Rust's `?`.
- `TRY` may appear only in functions that acquire nothing. This is greppable
  through section 3 naming: any function whose body calls `_create`, `_init`,
  `_open`, `alloc`, or any acquiring function uses explicit checks with
  explicit release instead.
- Prefer `static inline` functions over function-like macros in every case
  where types allow.

Why: a macro that hides control flow makes the visible code lie about its own
paths, so hidden control flow is banned wherever it could skip an obligation.
Orchestrators hold no obligations by construction, section 4, so a hidden
return inside one cannot leak anything. There `TRY` buys real safety: a
fallible call sitting outside `TRY` or an `if` becomes greppable as an
unchecked call, the single most common machine-written C bug. The cost is
priced in: a grep for `return` misses these exits, and a debugger steps into
the macro. Both are cheaper than the forgotten check the macro makes
impossible.

## 10. Duplication

- Logic never appears twice. The second occurrence becomes a named function
  at the moment it is written.
- A string literal used twice becomes a define.

Why: duplicated logic diverges under machine editing. A model patching one
copy has no link telling it the other copy exists. Single sourcing makes every
fix total.

## 11. Comments

- Above every prototype: one contract comment stating what the function does,
  ownership and nullability of every pointer, and the failure modes. Never
  restate the signature.
- Inside bodies: comment why, never what. The code says what.
- No commented-out code, ever. Version control remembers.

## 12. Formatting mechanics

- 4-space indent, no tabs. 100-column limit.
- One statement per line. One declaration per line.
- Function opening brace on its own line. Control-flow braces on the same
  line.
- A single-statement guard clause may omit braces with the statement on the
  next line. Everything else is braced.

## 13. Build hygiene

- Compiles clean under `-Wall -Wextra -Werror -Wconversion -Wshadow` from the
  first commit.
- C11 minimum. No compiler extensions without a wrapping macro and a comment.

## 14. Pre-delivery checklist

Before presenting any C code, verify:

1. Any literal that is not 0 or 1? Name it.
2. Any function over 40 lines or nested past depth 2? Split it.
3. Any contract comment containing "and"? Split the function.
4. Any `goto`? Decompose, or justify at the label.
5. Every fallible call checked and propagated?
6. Prototypes at the top of the file, matching every definition?
7. Each new error value: how many producers? Reduce toward one.
8. Any logic pasted twice? Extract it.
9. Any parameter list past 4? Struct it.
10. Header exposes only what callers need?
11. Any function mixing helper calls with inline logic? Push the logic into a
    leaf.
12. Any helper whose name paraphrases its body? Inline it.
13. `TRY` inside a function that acquires anything? Switch to explicit checks
    with explicit release.
14. Parameters out of context, outputs, inputs order? Reorder.
15. Any loop without a statically evident bound or a nonterminating marker?
    Bound it.
16. Any recursion? Convert to a bounded worklist loop.
17. Any state-mutating leaf with zero asserts? State the invariant.

## 15. Skeleton

```c
/* sensor.c: owns polling and conversion for the temperature sensor. */

#include <stdint.h>

#include "bus.h"
#include "sensor.h"

enum {
    SENSOR_POLL_INTERVAL_MS = 250,
    SENSOR_RAW_MAX          = 4095,
    SENSOR_SCALE_MILLIDEG   = 62
};

/* Propagates any non-OK status to the caller. Permitted only in
 * functions that acquire nothing. Sole macro allowed to return. */
#define SENSOR_TRY(expr)                            \
    do {                                            \
        sensor_status_t sensor_try_s_ = (expr);     \
        if (sensor_try_s_ != SENSOR_OK)             \
            return sensor_try_s_;                   \
    } while (0)

struct sensor {
    bus_t   *bus;       /* borrowed, never owned */
    int32_t  last_millideg;
};

/* Rejects a NULL sensor or output pointer. Fails with SENSOR_ERR_ARG. */
static sensor_status_t sensor_validate_poll_args(const sensor_t *s,
                                                 const int32_t *out_millideg);

/* Adapter over bus_read_u16. Fails with SENSOR_ERR_BUS. */
static sensor_status_t sensor_read_raw(sensor_t *s, uint16_t *out_raw);

/* Rejects raw samples above hardware range. Fails with SENSOR_ERR_RANGE. */
static sensor_status_t sensor_validate_raw(uint16_t raw);

/* Caches the converted sample on the sensor. Returns it in millidegrees. */
static int32_t sensor_record(sensor_t *s, uint16_t raw);

/* Converts a raw sample to millidegrees. Pure. */
static int32_t sensor_convert(uint16_t raw);

sensor_status_t sensor_poll(sensor_t *s, int32_t *out_millideg)
{
    SENSOR_TRY(sensor_validate_poll_args(s, out_millideg));

    uint16_t raw = 0;
    SENSOR_TRY(sensor_read_raw(s, &raw));
    SENSOR_TRY(sensor_validate_raw(raw));

    *out_millideg = sensor_record(s, raw);
    return SENSOR_OK;
}

static sensor_status_t sensor_validate_poll_args(const sensor_t *s,
                                                 const int32_t *out_millideg)
{
    if (s == NULL)
        return SENSOR_ERR_ARG;
    if (out_millideg == NULL)
        return SENSOR_ERR_ARG;
    return SENSOR_OK;
}

static sensor_status_t sensor_read_raw(sensor_t *s, uint16_t *out_raw)
{
    bus_status_t status = bus_read_u16(s->bus, SENSOR_REG_DATA, out_raw);
    if (status != BUS_OK)
        return SENSOR_ERR_BUS;
    return SENSOR_OK;
}

static sensor_status_t sensor_validate_raw(uint16_t raw)
{
    if (raw > SENSOR_RAW_MAX)
        return SENSOR_ERR_RANGE;
    return SENSOR_OK;
}

static int32_t sensor_record(sensor_t *s, uint16_t raw)
{
    s->last_millideg = sensor_convert(raw);
    return s->last_millideg;
}

static int32_t sensor_convert(uint16_t raw)
{
    return (int32_t)raw * SENSOR_SCALE_MILLIDEG;
}
```

Every rule above appears in this skeleton. `sensor_poll` is now six lines of
pure plan with zero propagation ceremony. Every leaf holds one piece of
logic, the adapter owns the only foreign call, and every error value has
exactly one producing line, so any failure greps to its cause in one step.
The file still teaches its complete vocabulary in the first screen.

## 16. Worked example: good but not good enough

Most machine-written C fails subtly, not grossly. This function passes generic
review: short, flat, guarded, no magic numbers. It still fails this standard.

```c
uint16_t map_eat(map_t *map, map_pos_t pos)
{
    map_cell_t cell;
    if (map == NULL)
        return 0;
    if (!map_is_inside(pos))
        return 0;
    cell = map->cells[pos.row][pos.col];
    if (cell == MAP_CELL_PELLET) {
        map->cells[pos.row][pos.col] = MAP_CELL_EMPTY;
        map->pellet_count--;
        return MAP_SCORE_PELLET;
    }
    if (cell == MAP_CELL_POWER) {
        map->cells[pos.row][pos.col] = MAP_CELL_EMPTY;
        map->pellet_count--;
        return MAP_SCORE_POWER;
    }
    return 0;
}
```

The tells, in order of weight:

1. The consume block is pasted twice. Clearing the cell and decrementing the
   count is one concept written in two places, section 10, checklist item 8.
   The moment the second branch was written, `map_consume_cell` should have
   been born. An editor adding a side effect to consumption, a sound cue or a
   dirty flag, will patch one copy and miss the other, because nothing links
   them.
2. The branches encode data as control flow. Cell type to score is a mapping,
   not logic. A mapping belongs in one lookup leaf, where the next cell type
   costs one line instead of one pasted block.
3. Three concepts interleave in one body, checklist item 11: deciding
   edibility, awarding score, and mutating the map. No single question about
   this function has a single home.
4. `map_cell_t cell;` sits uninitialized above the guards, section 7. Declare
   at first use.

The conforming decomposition, signature preserved:

```c
/* True when the cell can be eaten. Pure. */
static bool map_cell_is_edible(map_cell_t cell);

/* Score for consuming a cell. Zero for inedible cells. Pure. */
static uint16_t map_cell_score(map_cell_t cell);

/* Empties the cell and updates pellet accounting. */
static void map_consume_cell(map_t *map, map_pos_t pos);

uint16_t map_eat(map_t *map, map_pos_t pos)
{
    if (map == NULL)
        return 0;
    if (!map_is_inside(pos))
        return 0;

    map_cell_t cell = map->cells[pos.row][pos.col];
    if (!map_cell_is_edible(cell))
        return 0;

    map_consume_cell(map, pos);
    return map_cell_score(cell);
}

static bool map_cell_is_edible(map_cell_t cell)
{
    return cell == MAP_CELL_PELLET || cell == MAP_CELL_POWER;
}

static uint16_t map_cell_score(map_cell_t cell)
{
    switch (cell) {
    case MAP_CELL_PELLET:
        return MAP_SCORE_PELLET;
    case MAP_CELL_POWER:
        return MAP_SCORE_POWER;
    default:
        return 0;
    }
}

static void map_consume_cell(map_t *map, map_pos_t pos)
{
    map->cells[pos.row][pos.col] = MAP_CELL_EMPTY;
    map->pellet_count--;
}
```

The proof is change cost. Add a fruit cell: the original grows a third pasted
block, and the next editor patches two of three copies. The refactor grows one
line in `map_cell_is_edible` and one in `map_cell_score`. Grep improves the
same way: the question "what mutates cells" now has exactly one answer.

If edibility is exactly "scores nonzero", both pure leaves collapse further
into one `static const` score table indexed by cell type, zero branches. State
that invariant in a comment above the table if you take that step.

### The final stage

The stage above kept one violation on purpose: the signature fuses failure
with score, returning 0 for a NULL map, an out-of-bounds position, and an
ordinary empty cell alike. Pushed to full conformance, the module becomes two
files.

```c
/* map.h */
#ifndef MAP_H
#define MAP_H

#include <stddef.h>
#include <stdint.h>

enum {
    MAP_ROWS = 31,
    MAP_COLS = 28
};

typedef enum {
    MAP_CELL_EMPTY  = 0,
    MAP_CELL_WALL   = 1,
    MAP_CELL_PELLET = 2,
    MAP_CELL_POWER  = 3
} map_cell_t;

typedef enum {
    MAP_OK         = 0,
    MAP_ERR_ARG    = -1,
    MAP_ERR_BOUNDS = -2
} map_status_t;

enum {
    MAP_SCORE_PELLET = 10,
    MAP_SCORE_POWER  = 50
};

typedef struct {
    size_t row;
    size_t col;
} map_pos_t;

typedef struct {
    map_cell_t cells[MAP_ROWS][MAP_COLS];
    size_t     pellet_count;
} map_t;

/* Consumes the cell at pos if edible. Writes the score awarded,
 * zero when nothing edible is there. Fails with MAP_ERR_ARG on a
 * NULL pointer and MAP_ERR_BOUNDS on an out-of-range position. */
map_status_t map_eat(map_t *map, uint16_t *out_score, map_pos_t pos);

#endif /* MAP_H */
```

```c
/* map.c: owns the play grid, cell consumption, and pellet accounting. */

#include <assert.h>
#include <stdbool.h>

#include "map.h"

/* Propagates any non-OK status to the caller. Permitted only in
 * functions that acquire nothing. Sole macro allowed to return. */
#define MAP_TRY(expr)                       \
    do {                                    \
        map_status_t map_try_s_ = (expr);   \
        if (map_try_s_ != MAP_OK)           \
            return map_try_s_;              \
    } while (0)

/* Rejects NULL map or output pointers. Fails with MAP_ERR_ARG. */
static map_status_t map_validate_eat_args(const map_t *map,
                                          const uint16_t *out_score);

/* Rejects positions outside the grid. Fails with MAP_ERR_BOUNDS. */
static map_status_t map_validate_pos(map_pos_t pos);

/* Consumes the cell at pos if edible. Returns the score awarded,
 * zero otherwise. */
static uint16_t map_consume_at(map_t *map, map_pos_t pos);

/* True when the cell can be eaten. Pure. */
static bool map_cell_is_edible(map_cell_t cell);

/* Score for consuming a cell. Zero for inedible cells. Pure. */
static uint16_t map_cell_score(map_cell_t cell);

/* Empties the cell and updates pellet accounting. */
static void map_consume_cell(map_t *map, map_pos_t pos);

/* The only two functions that touch cell storage. */
static map_cell_t map_cell_at(const map_t *map, map_pos_t pos);
static void map_set_cell(map_t *map, map_pos_t pos, map_cell_t cell);

map_status_t map_eat(map_t *map, uint16_t *out_score, map_pos_t pos)
{
    MAP_TRY(map_validate_eat_args(map, out_score));
    MAP_TRY(map_validate_pos(pos));

    *out_score = map_consume_at(map, pos);
    return MAP_OK;
}

static map_status_t map_validate_eat_args(const map_t *map,
                                          const uint16_t *out_score)
{
    if (map == NULL)
        return MAP_ERR_ARG;
    if (out_score == NULL)
        return MAP_ERR_ARG;
    return MAP_OK;
}

static map_status_t map_validate_pos(map_pos_t pos)
{
    if (pos.row >= (size_t)MAP_ROWS)
        return MAP_ERR_BOUNDS;
    if (pos.col >= (size_t)MAP_COLS)
        return MAP_ERR_BOUNDS;
    return MAP_OK;
}

static uint16_t map_consume_at(map_t *map, map_pos_t pos)
{
    map_cell_t cell = map_cell_at(map, pos);
    if (!map_cell_is_edible(cell))
        return 0;

    map_consume_cell(map, pos);
    return map_cell_score(cell);
}

static bool map_cell_is_edible(map_cell_t cell)
{
    return cell == MAP_CELL_PELLET || cell == MAP_CELL_POWER;
}

static uint16_t map_cell_score(map_cell_t cell)
{
    switch (cell) {
    case MAP_CELL_PELLET:
        return MAP_SCORE_PELLET;
    case MAP_CELL_POWER:
        return MAP_SCORE_POWER;
    default:
        return 0;
    }
}

static void map_consume_cell(map_t *map, map_pos_t pos)
{
    assert(map->pellet_count > 0);
    map_set_cell(map, pos, MAP_CELL_EMPTY);
    map->pellet_count--;
}

static map_cell_t map_cell_at(const map_t *map, map_pos_t pos)
{
    return map->cells[pos.row][pos.col];
}

static void map_set_cell(map_t *map, map_pos_t pos, map_cell_t cell)
{
    map->cells[pos.row][pos.col] = cell;
}
```

What the final stage bought, item by item. Every failure now has a name and
exactly one producing validator, and normal gameplay never wears an error's
clothes: an empty cell is `MAP_OK` with score zero. `MAP_TRY` collapses the
propagation, safe because `map_eat` acquires nothing. `map_cell_at` and
`map_set_cell` own every touch of cell storage, so the row-major indexing
convention lives in two adjacent lines and the question "what mutates cells"
greps to one. `map_consume_cell` asserts the accounting invariant instead of
re-validating, section 8: the public boundary already proved the arguments.
Parameter order follows context, outputs, inputs throughout.

This stage also forced two amendments to section 4: orchestrators may branch
on named predicates, and leaves may call the module's accessors. That is the
meta-lesson of this whole example. A standard is not written once, it is grown
by feeding it code that breaks it, and each break becomes a rule or an
amendment. Apply the same process to any codebase adopting this document.

## 17. The repository level

Function and file rules are half the job. Agents navigate repositories, and
the repository has its own legibility budget.

- An `AGENTS.md` at the repo root, under 150 lines, hand-written. It carries
  exactly what a capable stranger could not infer: build, test, and lint
  commands, the boundaries an agent must not cross, and the two or three
  architectural decisions that look wrong from outside but are intentional.
  Field studies of agent task completion converge on the same discipline:
  short beats comprehensive, and a section earns its place only after an
  agent has repeatedly gotten that thing wrong.
- Resolve either-or conventions with decision tables. When the codebase
  sanctions two ways to do a thing, a table choosing per situation beats
  prose and measurably improves convention adherence.
- Include a few short examples lifted from the real codebase, three to ten
  lines each. Agents pattern-match, so hand them the right pattern. More than
  a handful and they start matching the wrong thing.
- Describe capabilities and shape, not file paths. Paths rot, and a
  confidently wrong map is worse than none. Per-directory purpose lives in a
  one-paragraph README inside that directory, which agents read first when
  they list it.
- A nested `AGENTS.md` per package carries anything package-specific.
- The test suite is the agent's feedback loop. An agent works autonomously
  exactly as far as the tests can verify; past that point every change routes
  through a human. Tests are part of the readability story, not separate from
  it.
- Verbosity is a tax on context. Everything an agent must read to act
  competes with the task itself for the same window. Density of signal is a
  repository property, and this whole document is in its service.

## 18. Provenance

Every rule here either survived contact with working code or was taken from
a source that earned it. The sources, and the one thing each contributed:

- Gerard Holzmann, The Power of 10, NASA/JPL, 2006. Bounded loops, the
  recursion ban, the two-asserts-per-function density floor, smallest scope,
  and checking every return. Its ban on dynamic allocation after
  initialization is flight-software law, not general practice, and is not
  adopted. <https://spinroot.com/gerard/pdf/P10.pdf>
- G. Ann Campbell, Cognitive Complexity, SonarSource, 2017, revised 2023.
  The formal case for the depth cap: the metric charges each break in linear
  flow and charges nesting progressively with depth, and maintainers accepted
  its verdicts at a 77 percent rate in the field.
  <https://www.sonarsource.com/docs/CognitiveComplexity.pdf>
- Feitelson et al., identifier naming studies, ICPC line of work. Good names
  carry up to a 30 percent comprehension effect in humans. Names are the
  code's documentation.
- When Names Disappear, 2025, arXiv 2510.03178. Stripping identifiers
  degrades LLMs even on execution tasks that should depend only on structure.
  Naming is a first-class semantic channel for models, which is why section 3
  exists in its current strength.
- Li et al., What Builds Effective In-Context Examples for Code Generation,
  2025, arXiv 2508.06414. Models generate better against precise concise
  identifiers than verbose composites. Precision beats length, section 3.
- Rethinking Code Complexity Through the Lens of Large Language Models, 2026,
  arXiv 2602.07882. Model-perceived complexity, driven by semantic hierarchy
  depth and branching breadth, correlates strongly with task performance
  after controlling for code length, and semantics-preserving rewrites that
  reduce it improve downstream results by up to roughly 21 percent. The
  direct experimental support for this document's premise.
- Enhancing LLM-Based Code Generation with Complexity Metrics, 2025, arXiv
  2505.23953. Standard complexity metrics predict whether generated code
  passes, and complexity feedback improves regeneration. The reason section
  4 carries a numeric budget.
- The `AGENTS.md` convention, agents.md, plus 2026 field reports on agent
  one-shot task completion. The repository rules of section 17.
- Kernighan and Pike, The Practice of Programming, 1999. The lineage:
  simplicity, clarity, generality, in that order.
- David Hanson, C Interfaces and Implementations, 1996. The
  module-as-interface discipline behind sections 1 and 7, and ownership
  stated at the interface.
- MISRA C and CERT C. Adopted in spirit: no reliance on undefined behavior,
  every warning an error. Rejected explicitly: the single-exit-point rule,
  because early returns are what keep cognitive complexity low and what
  `TRY` depends on.
