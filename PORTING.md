# Rogue 3.4 — macOS / Linux port: technical history and validation

## Scope and status

The original archive contains a reconstruction of Berkeley Rogue 3.4 based on
the 1981 executable. The historical reconstruction remains the **reference**.
This directory contains a staged portability implementation, not an attempt to
reproduce the original binary bit-for-bit. The chronological record below
is followed by a consolidated original-rules audit. Native macOS compilation
and game operation have been confirmed by the maintainer; the original Linux
regression records below remain labeled as Linux tests. Detailed test coverage
on each Mac configuration is not documented here.

For focused reference, see the [original-rules audit](#original-rules-audit).
The scoring implementation is not re-audited in this consolidation.

### Current fidelity policy (after Stage 28)

The primary goal is to reproduce the reconstructed 1981 Rogue 3.4 **gameplay
behavior**, rather than modernize the game rules. Memory safety, undefined
behavior, nonterminating loops and operating-system compatibility still need
safe solutions, with ordinary in-range behavior and random-number consumption
preserved wherever possible.

- `potions.c` (`P_CONFUSE`): restored the reconstructed source's behavior.
  A second confusion potion while already confused does **not** extend the
  duration, print the initial-confusion message, or draw a random number.
  The Stage 14 adjustment described below has been superseded.
- `options.c` (`parse_opts`): restored case-sensitive, first-match prefix
  abbreviations for option names, including `no` prefixes for boolean options.
  Modern length bounds and filesystem-safe filename handling remain in place.
  The Stage 2 exact-name restriction described below has been superseded.
- Historical Stage 14 and Stage 19 test results remain recorded as originally
  performed; their earlier repeated-confusion difference no longer describes
  the current source. This change has not been validated by rerunning those
  historical suites.

### Previous changes retained (stages 1 and 1b)

- Replaced PDP-11, SCCS and obsolete termlib build rules with a direct
  Clang/curses-compatible Makefile (`make`, `make clean`). GNU C89 mode is
  retained temporarily for some historical return-value conventions.
- Consolidated header global *definitions* in `globals.c` while preserving
  initialized definitions in the original source files (`init.c`, etc.).
- Corrected `CTRL` key handling by using character literals in call sites.
- Replaced removed curses internals (`_tty`, `_scroll`, `_unctrl`, `_doprnt`,
  `crmode`) with public curses functions, termios helpers, and `vsnprintf`.
- Renamed gameplay functions that clashed with system functions, and fixed
  code that relied on the obsolete return-value convention for `sprintf`.
- Made potion color strings and the violet fungi damage description writable.
- Used unsigned arithmetic in the original RNG to preserve the intended
  low-order bits without signed-overflow undefined behavior.
- Fixed the internal playfield at 80×24; require a terminal at least that size.
- Placed the score file in the user's home directory at `~/.rogue34.scr`.
- Disabled the original save/restore code because it writes raw program
  memory and is not compatible with a modern process address space.
- Removed load-based gameplay restrictions intended for historical
  time-sharing computers.
- Converted **143 K&R-style function definitions** to parameter-typed
  definitions, and replaced obsolete function declarations with prototypes
  in `rogue.h`. Removed conflicting declarations for `getpwuid`, `getpass`,
  `crypt`, and `index` (now using `strchr`).
- Fixed missing third arguments to `score()` for the score-only, quit,
  and winner paths; fixed the no-op unsigned `abs(RN)` call.
- Made daemon/fuse callback types explicit. Current scheduled callbacks
  take no arguments; calling them with the historical extra integer is
  no longer necessary and was not prototype-safe.
- Typed options callbacks and removed their legacy non-prototype
  declarations. Added braces where nested `if/else` associations were
  ambiguous and made the intended wizard password assignment explicit.
- Normalized `chtype` values obtained from curses to printable game
  characters, ignoring curses attributes and rejecting the `ERR` sentinel.
- Switched the shell wait to `waitpid()` and used a correct pointer sentinel
  for `execl()`. Invalid passage directions now return immediately rather
  than using uninitialized destination values.
- This is a compatibility and warning-cleanup revision of the previous
  stage, not an intentional change to Rogue 3.4 gameplay mechanics.

### Stage 2: runtime correctness and playtesting

- Fixed a **heap-use-after-free** in `do_daemons()` discovered with AddressSanitizer
  after repeated turns: the `rollwand` callback can remove its own daemon node.
  The next-list pointer is now captured before invoking a daemon callback.
- Rewrote the `ROGUEOPTS` string parser with exact option-name matching and
  length-bounded copying, covering `name`, `fruit`, and `file`. Interactive
  option editing also uses bounded copying. Deliberately retain the original
  80-byte option fields and ASCII-oriented option semantics.
- Stopped treating player-influenced strings as `printf` format strings in
  combat messages, inventory, replay-last-message, and trap descriptions;
  enlarged the inventory text buffer and the shared item-description buffer.
- Handle end-of-input and hard read errors without an infinite input loop;
  `EINTR` retries remain supported. Constrain the top-line message and
  `--More--` marker to the fixed 80-column command area.
- Use explicit coordinates for stairs/traps rather than incidental curses
  cursor movement, and avoid dereferencing a nonexistent monster entry in
  `show()`; avoid leaking an allocation in `wanderer()` error handling.
- Correct status-line caching when maximum HP changes, and clear residual
  text when switching a boolean option from `False` to `True`.
- Simplified terminal restoration after SIGTSTP to re-enable `cbreak`/`noecho`;
  more extensive signal-safety review is still outstanding.

### Stage-2 validation in this environment

- Clang compile and link using the analysis-only ncurses declaration shim.
  The strict-prototype/deprecated-non-prototype/implicit-declaration build
  completed with **0 warnings**. This is *not* an exhaustive `-Wall -Wextra`
  clean build (historical initializers and code still produce other warnings).
- AddressSanitizer + UndefinedBehaviorSanitizer pseudo-terminal scenarios:
  inventory, help, options, move/search, item-action cancellation, score,
  and extremely long option strings; all four scripted scenarios exited
  normally after the `do_daemons` fix, without reported sanitizer errors.
- Standalone `-s` creates the per-user score file and displays the table;
  23×79 terminal is rejected; a 40×120 terminal runs the fixed 80×24 game.
- A separate non-distributed, instrumented test checked one staircase per
  level across 41 consecutive levels twice, with ASan/UBSan enabled. Another
  trial paused on the game's `--More--` prompt and was not counted as passed.
- Extra random-movement stress trials were inconclusive if they ended at an
  input prompt and required forced termination. No further sanitizer error
  was observed in those trials, but **combat, death, victory, and complete
  pathfinding between rooms are not yet fully verified**.

### Stage 3: stairs, combat, death, victory, and portable score file

- Introduced a versioned ASCII score format at `~/.rogue34.scr` instead of
  writing the original in-memory C `struct` representation. The score
  filename remains the same as in stages 1 and 2.
- Preserved Rogue 3.4's top-ten insertion order, strict `>` comparison,
  wizard-mode exclusion, and the original death/quit/victory score rules.
  The `-s` command now reads without creating or rewriting the score file.
- Parse and validate the whole file before modification; reject unknown
  versions, invalid records, unexpected trailing content, excessive length,
  and non-regular files. Names are hex encoded in the file and displayed as
  printable text, preventing terminal-control and delimiter injection.
- Serialize concurrent score readers/writers with `flock()`. Acquire the
  lock *before* inspecting the file size. Writes are flushed and `fsync()`ed.
  The file may still be incomplete if a process is killed or storage fails
  **during** a write; atomic crash-safe replacement is not implemented.
  An invalid file is not rewritten automatically.
- Reset `ntraps` when generating each level; older trap coordinates must not
  remain logically active on a level that creates no new traps.
- Fix tombstone year display: the 1981 code changed only the last two digits
  of the static `1980` stone, resulting in `1926` during 2026. Display all
  four digits, and constrain long names to the stone's width.

### Stage-3 validation in this environment

- Test-only source instrumentation was used to reach specific paths without
  playing through all the intervening floors; **no test hooks ship in the ZIP**.
- Pseudo-terminal + ASan/UBSan checks completed on Linux: normal quit,
  command/inventory input, stairs descent (`>` and level-2 display), an
  explicit death call, death through a high-damage monster attack, direct
  monster combat, and the amulet-assisted ascent/winner path (`<`).
- A valid score is created and read back, while score-only `-s` leaves files
  unchanged. Unknown score formats are rejected without rewriting.
- Twelve concurrent score submissions were tested, and the final top-ten
  file contained the ten highest values in descending order. The full test
  used a Linux compatibility header shim.
- Forty additional standalone starts reached a rendered dungeon and exited
  normally in a bounded Linux pseudo-terminal stress sample.
- This validates specific code paths, **not** fully natural pathfinding,
  all dungeon layouts, complete combat statistics, or an unassisted win.

### Score filename correction

- Restored `~/.rogue34.scr` (fallback: `rogue34.scr`) as the filename;
  the versioned score format, locking and score rules are unchanged.
- No existing-score migration or conversion was added.

### Features and limitations

- **Save and restore are implemented in stage 4** using a separate versioned
  state format. Only the historical raw-memory save files remain unsupported.
  SIGHUP/SIGTERM auto-save is still disabled because saving complex state
  from a POSIX signal handler would be unsafe.
- All active K&R-style function **definitions** have been changed to typed
  definitions and the previously reported deprecated-non-prototype,
  ambiguous-else, unsigned-abs and curses-character warnings were addressed.
  **Stage 5 now enables C11 and `-Wall -Wextra -Wpedantic` by default**,
  including explicit returns where older routines fell off the end.
  This has also been checked on Linux against Clang 17 with `-Werror`.
- Scores use the new portable file above. Crash-safe *atomic* writes, broader
  input validation, and independent platform checks remain future work.
- Exhaustive, all-path gameplay testing has not been completed.
- The historical manual and guide are supplied unchanged. Their details
  of the 1981 save implementation do not describe this port's new format.

### Build instructions

macOS (Apple Command Line Tools installed, no Homebrew dependency intended):

```sh
cd src
make
./rogue
./rogue -s
make clean
```

Linux: ncurses development headers, curses library and libcrypt development
libraries may be required; `make` is supported when these are installed.

### Validation actually performed

The updated source was compiled and linked with Clang on Linux, including a
check with `-Wstrict-prototypes`, `-Wdeprecated-non-prototype`,
`-Wimplicit-function-declaration` and `-Wimplicit-int` (and error flags
for the latter three). This limited diagnostic build emitted zero warnings
and zero errors. A basic pseudo-terminal test also passed under
AddressSanitizer/UndefinedBehaviorSanitizer.

Because this environment
lacked the ncurses development header, testing used an **analysis-only**
curses declaration shim matched to the installed ncurses runtime library,
not the headers provided with macOS. In a pseudo-terminal, a new 80×24 game
started, a dungeon appeared, and the `Q` / `y` / Return sequence produced
normal termination and a score display. A subsequent AddressSanitizer and
UndefinedBehaviorSanitizer smoke run reported no issue in that limited path.


### Stage 4: versioned portable save / restore

- `S` asks whether to save to the configured `file` option. On success it
  exits; on error the game continues and the existing save is not overwritten.
- A `-r` argument resumes the default/configured save; an explicit filename
  resumes that file. Normal successful restore consumes the file (wizard-mode
  sessions retain it); an invalid or truncated file is never removed.
- The v1 format is `R34SAV01` followed by a fixed-width payload length and a
  32-bit FNV-1a integrity check, with bounded encoded fields. It saves player
  and monster attributes, item lists and equipped references, dungeon rooms and
  traps, identification state, random seed, all three curses playfield layers,
  deferred daemon/fuse callbacks by stable symbolic IDs, and the wandering
  monster countdown. No pointer values or native structs are serialized.
- Temp-file + flush + `fsync()` + `rename()` avoids truncating a good save when
  the new write fails. In-progress crashes after a rename are not proven durable
  across all filesystems; stage 6 adds best-effort parent-directory `fsync()`.
- Recreated the mutable violet-fungi damage string at program startup and on
  restore, and moved the history-dependent command/run counters from file-local
  statics so they are preserved in the saved state. `ROGUEOPTS` is not re-applied
  to an already-restored run.
- **Limitations:** original 1981 memory-image save files are not compatible;
  there is no automatic SIGHUP/SIGTERM save. The v1 format is intended for this
  port, not cross-version interoperability or untrusted-file security.
- Linux pseudo-terminal tests covered initial save (`S`), `-r` restore, restored
  inventory, resave, and rejection/retention of a modified file. Clang + ASan/
  UBSan detected a `strcpy()` overlap bug in the `-r` path and the path is now
  copied to stable storage before decoding. Repeated save/restore tests completed
  without sanitizer diagnostics on the tested paths. These Linux tests used a
  temporary development-only ncurses declaration shim (not bundled).
- The scores remain in `~/.rogue34.scr` with no migration/conversion handling.

### Stage 4: tombstone and score screen transition correction

- A macOS Terminal screenshot showed the shell's preceding compile output
  with `[Press return to continue]` instead of the death tombstone. The
  death handler had refreshed the tombstone, but `score()` immediately called
  `endwin()`, switching out of curses' alternate screen before waiting.
- Separated the screen-level Return pause from score I/O. `death()` now
  renders the original tombstone, puts the prompt at row 24 of the 80×24
  curses screen, waits for Return there, and **then** calls `endwin()` and
  displays the top-ten list. A confirmed `Q` quit uses the same in-game
  pause before printing the list. The victory screen keeps its congratulatory
  text on the ordinary terminal screen and waits once before printing scores.
- `score()` no longer invokes curses, changes terminal modes, or asks for
  input; callers end curses before scoring, and `rogue -s` works without
  initializing curses. Score file `~/.rogue34.scr`, its versioned data
  format, and the stage-4 save/restart data format are unchanged.
- On Linux, PTY checks with `TERM=xterm` verified that no alternate-screen
  exit sequence is written before Return on either death or `Q`, that the
  tombstone and prompt are visible before Return, and that scoring appears
  after it. The winner path was also exercised with a **test-only** command;
  the injected commands are not included in this ZIP. Quit/death/win smoke
  tests also passed with ASan/UBSan. Three save/restore cycles and score-only
  display still passed.

### Stage 5: C11 and warning cleanup

- Switched the default C standard from GNU C89 to C11, enabling `-Wall`,
  `-Wextra` and `-Wpedantic` without suppressing `-Wreturn-type`.
- All original functions already had modern parameter lists from stage 1b;
  stage 5 supplies explicit `return 0;` for historical `int` routines that
  previously returned no value, including early exits. This preserves
  their existing signatures (notably daemon callbacks and function prototypes).
  A later API redesign might use `void` where return values are never used.
- Wrapped the `helpstr` and weapon-initialization records in proper
  structure initializer braces. Removed unused variables and made array
  indexing through legacy `char` values unsigned where appropriate.
  Added parentheses to a combined logical expression for clarity.
- Clarified intentional control flow through shared switch-case bodies in
  item pickup, inventory, movement, and victory loot appraisal. The cases
  perform the same operations as before, including the historical FOOD
  valuation behavior on the winner path.
- Initialized legacy temporary variables used in randomized monster
  disguises and trap types. The wandering-monster retry loop formerly had
  an uninitialized character if it skipped the hero's room before a first
  valid tile was inspected; it now starts with a non-walkable value.
- Replaced truncating `snprintf("%s")` copies into the 80-byte history and
  20-byte tombstone name buffers with explicitly bounded copies.
- Set `_DARWIN_C_SOURCE` on macOS and `_DEFAULT_SOURCE` on Linux to
  request each platform's extension declarations under strict C11.
- **Validation:** Clang 17 and GCC 14 on Linux each built and linked the complete
  program with `-std=c11 -Wall -Wextra -Wpedantic -Werror`: zero warnings
  and errors.
  Tests used an external analysis-only curses declaration shim (not bundled),
  because this runtime lacks the ncurses development headers. The GNU/Linux
  runtime library was available for executing the tests.
- Linux PTY regressions passed: confirmed `Q` quit, in-game tombstone/Return
  sequencing, winning/Return sequencing (test-only commands), score display,
  and three save/resume cycles. The same scenarios completed under
  AddressSanitizer + UndefinedBehaviorSanitizer (`detect_leaks=0`) without
  sanitizer diagnostics. The distributed sources have no test hooks.
- No modification to `R34SAV01` save bytes or version, score serialization,
  `~/.rogue34.scr` filename, or the original game mechanics was intended.

### Stage 6: restore validation, save durability and stress testing

- Preserve the `R34SAV01` header, payload and checksum algorithms. Existing
  stage-4/5 saves can still be read; there is no file conversion. The score
  filename remains `~/.rogue34.scr`, with no special legacy-data handling.
- Validate decoded room positions, dimensions, active exits and gold positions
  before using them for game arithmetic or curses calls. Check active trap
  types, object subtypes (including food/amulet), one-byte fields, short
  integer fields, player health, and monster statistics. The room-bounds
  validation avoids signed-overflow arithmetic even with crafted INT_MAX
  dimensions. Failed decoding leaves the original save file intact.
- Preserve the exact serialized RNG seed when calling `init_weapon()` to
  rebuild an item template; missile initialization can otherwise consume
  random numbers while the saved inventory is reconstructed. A test fixture
  changed the initial equipped sword into a valid arrow item and verified that
  restore and immediate resave produce the same RNG seed.
- Hold an advisory exclusive `flock()` from the start of the restore read
  until the restored file has been consumed. Refuse competing simultaneous
  restores and compare the opened descriptor's identity with the path before
  removal, so common pathname replacement races are detected. These checks
  are not an atomic compare-and-delete primitive and cannot guarantee
  protection from a hostile actor replacing the path between check and unlink.
- Continue to write a new save into a same-directory `mkstemp()` file, flush
  and `fsync()` the contents before atomic `rename()`. A best-effort parent
  directory fsync is performed after rename; some filesystems do not support
  directory fsync, and failure after a committed rename is not reported as
  failure to save. Preserve the original I/O error before cleaning a failed
  temporary write. `S` continues the game after a failed write.
- **Validation (Linux only):** Clang 17 and GCC 14 accepted all source files
  with C11, `-Wall -Wextra -Wpedantic -Werror` and no warnings. Under Clang
  ASan/UBSan (with leak detection disabled), tested three save/restore cycles,
  10 deterministic invalid-save examples and 100 random single-byte save
  changes with the FNV checksum recomputed. A separate 10-round two-process
  simultaneous-restore test allowed one restore per saved game. An unprivileged
  read-only-directory test confirmed a failed save does not replace an
  existing file and the game remains running. A 16-launch PTY input stress
  test injected 8,076 movement keystrokes (not 8,076 confirmed game turns).
  Quit/death/win terminal transitions passed, including test-only death/win
  hooks. These tests are not shipped and are not a complete game playthrough.
- A noncryptographic checksum and semantic checks are **not an authenticity
  mechanism**. Do not load untrusted save files. Very rare concurrent file
  replacement races and filesystem-specific power-loss guarantees remain
  outside this test scope.

### Stage 7: modern-system cleanup and source dependencies

- Removed the unused Berkeley timesharing load restrictions, time-based load
  checks, `LOADAV`/`UCOUNT` support and the port's `mach_dep.h` header. The
  modern Makefile has always compiled these paths out; no game rule or runtime
  condition used by the distributed build was removed.
- Removed the obsolete `UCBHASH` environment-variable warning and the dead
  low-baud-rate terminal-settings branch (`if (0)`). Neither had been active in
  normal builds.
- Eliminated a second `ROGUEOPTS` parsing pass inside `playit()`. Options are
  already parsed once by `main()` before starting or restoring the game; the
  restore path reads its option values from `R34SAV01`. Removed the resulting
  unused `rogue_restored` switch; it is not part of the serialized data.
- Made `rogue.h` include its curses dependency and have a header guard,
  removed obsolete load-related prototypes and an unused string macro.
  The tombstone's template-string pointers were made read-only.
- Replaced the misleading `auto_save()` no-op stub with an explicit emergency
  signal exit in `compat.c`. SIGHUP and SIGTERM still call `_exit(1)` without
  attempting to save, as in stages 4–6; an interactive `S` is still required.
  Signal-handling design beyond this naming cleanup was not changed.
- Corrected an outdated comment about the starting weapon; the existing
  starting equipment is unchanged. No new source files, gameplay flags,
  platform requirements, or serialization fields were introduced.

**Validation (Linux only):** Clang and GCC compile and link the full program
with C11, `-Wall -Wextra -Wpedantic -Werror`, using a test-only compatibility
header for the unavailable ncurses development headers. PTY tests exercised
three save/restart cycles, scoreboard lookup, normal quit, death/tombstone,
victory transitions, ten two-process concurrent restores and exact RNG-seed
preservation after missile-weapon reconstruction. A stage-6-produced
`R34SAV01` file was also opened and saved again by stage 7, and SIGTERM
retained the prior emergency exit behavior without auto-saving. ASan/UBSan
checks on the exercised save, display and RNG paths reported no diagnostics;
they do not cover a complete playthrough. No bundled test shim or debugging
instrumentation is present in the distribution.

**Unchanged:** `~/.rogue34.scr`, `~/rogue.save`, `R34SAV01`, the winner/quit/
death score rules and ASCII tombstone, game mechanics and RNG algorithm.

### Stage 8: terminal suspension, shell escape and deferred signal handling

- `SIGINT`, `SIGQUIT`, `SIGTSTP`, `SIGHUP` and `SIGTERM` handlers now set only
  `volatile sig_atomic_t` flags. The game processes them in the ordinary
  control flow, from the input loop or the main game loop, instead of calling
  curses, stdio, `quit()` or `endit()` inside a signal handler.
  `sigaction()` is used without `SA_RESTART` so a blocking terminal `read()`
  can handle pending signals as soon as it returns with `EINTR`.
- SIGINT continues to ask `Really quit?`. Additional SIGINTs during that
  question do not recursively open another dialog. Termination signals still
  exit and restore the terminal while the question is displayed.
- SIGHUP/SIGTERM no longer use the signal handler's immediate `_exit(1)`:
  when dispatched in normal execution they call `endwin()`, report the
  signal and exit with `128 + signo`. **No automatic save is attempted.**
  The OS cannot guarantee cleanup on SIGKILL or a fatal crash.
- SIGTSTP now leaves the curses screen at a safe point, stops via SIGSTOP,
  and redraws the game after SIGCONT. This avoids calling curses from a
  signal handler and preserves the original 80x24 internal screen.
- `!` shell escape now uses one `fork()`, handles fork/exec failures without
  an infinite retry, restores the original SIGINT/SIGQUIT/SIGTSTP dispositions
  in the parent, and leaves the child with default interactive dispositions.
  After the child exits, Return resumes the game with a full curses redraw.
- Wizard password input uses the game's byte-reading routine rather than
  mixing buffered stdio input with unbuffered terminal reads.
- Kept `R34SAV01`, the save location (`~/rogue.save`), the score file
  (`~/.rogue34.scr`), the score serialization and game mechanics unchanged.

**Validation (Linux only):** Clang 17 and GCC 14 accepted C11,
`-Wall -Wextra -Wpedantic -Werror` with zero warnings, using a test-only curses
header shim because real ncurses development headers were unavailable in the
validation environment. With the actual ncurses runtime and Linux PTYs,
three consecutive SIGTSTP/SIGCONT suspend/resume cycles restored the alternate
screen and allowed movement/inventory and quit; SIGINT quit/cancel and a second
quit worked; SIGHUP/SIGTERM restored the original terminal settings (ICANON,
ECHO) and left the alternate screen without saving; SIGQUIT, `!` shell escape,
and a nonexistent shell executable were handled without hangs. Additional
checks covered SIGINT during an already pending quit prompt and SIGTERM while
that prompt was active. Existing stage-7 save/restore, score, tombstone,
win-screen, concurrent-restore and RNG tests passed. AddressSanitizer and
UndefinedBehaviorSanitizer (`detect_leaks=0`) reported no errors along the
exercised paths. SIGTSTP was simulated by sending the signal to a Linux
PTY process rather than by keyboard input. No test-only headers or
binaries are distributed.

### Stage 9: SIGWINCH and fixed-size terminal redraw, plus stress checks

- Added a SIGWINCH handler that **only sets a `sig_atomic_t` flag**, and checks
  actual dimensions with `ioctl(TIOCGWINSZ)` in normal execution (`readchar`,
  main game loop and refresh paths). No curses calls are made in the handler.
- Added a draw wrapper that tracks the last visible WINDOW, including the
  inventory/options overlay and the tombstone, so it can be repainted when
  the terminal dimensions change while a prompt is awaiting input.
- Supported terminal sizes of at least 80x24 may vary freely; `resizeterm()`
  updates `stdscr`/`curscr` bookkeeping. The game keeps the original fixed
  80x24 `cw`/`mw`/`hw` windows rather than enlarging the dungeon.
- When the physical terminal is smaller than 80x24, leave curses' alternate
  screen, print an enlargement notice and suspend normal gameplay/input until
  a suitable size returns. **Do not call `resizeterm()` at the undersized
  dimensions:** ncurses can otherwise truncate the fixed 80x24 windows, whose
  cells are part of Rogue 3.4's dungeon state. SIGTERM and SIGTSTP are still
  processed during the resize wait. Once resized, restore program mode and
  repaint without consuming a game command. SIGINT is deferred until the
  normal screen is available again.
- Resizes while in the `!` shell are applied when the game screen resumes.
  The screen is repainted after resuming a Ctrl-Z suspension as well.
- Existing `~/.rogue34.scr`, `~/rogue.save`, `R34SAV01`, scoring, RNG,
  tombstone, game rules and user options were not modified.

**Validation (Linux PTY):** Clang and GCC strict C11 builds
(`-Wall -Wextra -Wpedantic -Werror`) passed with zero warnings using a
**test-only** ncurses declarations shim and local library-name symlinks.
Three enlarge → undersize → restore cycles succeeded; after resizing, inventory
and quit still worked. A too-small window did not advance the game; SIGTERM
restored normal terminal modes. Additional cases exercised an inventory overlay
across resizes, saving and restoring after resizing, resizing during an
interactive `!` shell and during Ctrl-Z suspension. Stage-8 signal tests,
three save/restart cycles, tombstone, win and quit screen transitions also
passed. Twelve scripted command-stress sessions delivered 14,100 keys on a
normal build and 15,100 keys on an ASan/UBSan build without an unexpected
process exit or sanitizer diagnostics; those numbers are **keys sent**, not
verified game turns. These are limited scripted paths, not a full playthrough,
and the distribution contains no test-only tools, headers or binaries.

### Stage 10: preserve 80×24 game screens across resume and resizing

The stage-9 implementation assumed that, after `endwin()`, calling
`reset_prog_mode()` would make `isendwin()` return false. This is not the
contract of curses: `isendwin()` remains true until a subsequent `wrefresh()`.
As a consequence, `rogue_resume_screen()` could skip the physical size check
and redraw into a too-small terminal after `fg` or `!` shell escape.

- Added a scoped resume check that runs while curses still reports an
  `endwin()` state, **before** refreshing any game window. If physical
  dimensions are under 80×24, it stays out of the game screen until the
  terminal is enlarged; otherwise, it updates curses' size bookkeeping.
- Avoids redundant `endwin()` when a small terminal is detected while already
  in shell mode. Preserves `SIGTERM` handling during size-wait as before.
- Apple documents that `resizeterm()` attempts to resize *all* curses windows,
  so after every successful size update the code checks the dimensions of
  `cw`, `mw`, and `hw`; any changed window is resized back to exactly 80×24
  with public `wresize()`. A failure aborts after `endwin()` rather than
  continuing with a corrupted map. The window's original top-left cells are
  kept, and the active window is redrawn after the resize.
- No changes were made to the 1981 game rules, `R34SAV01` serialized format,
  `~/rogue.save`, `~/.rogue34.scr`, RNG, scoring or tombstone.

**Validation (Linux only):** Strict Clang 17 and GCC 14 C11 builds passed with
`-Wall -Wextra -Wpedantic -Werror` and no warnings using a **test-only** curses
header shim linked to installed runtime libraries. A PTY test confirmed that
a terminal shrunk to 72×19 during SIGSTOP/SIGCONT is rejected **before** any
`Level:` display, then resumes at 80×24. Saving and restarting after this
resume worked. A second PTY test shrank the terminal while in the interactive
`!` shell, exited the shell, and confirmed the same warning/recovery sequence.
Normal 70×18 shrink/recovery, repeated 100–120-column enlargements and
80×24 restoration, SIGTERM during undersized pause, and SIGINT cancellation
followed by a resize also passed. The new paths passed Clang
AddressSanitizer/UndefinedBehaviorSanitizer checks (`detect_leaks=0`). The generated ZIP excludes all test-only files and binaries.

### Stage 11: conservative dead-code and dependency cleanup

- Removed the unreferenced `Decho(int)` demo daemon and its public prototype.
  The actual daemon/fuse callbacks and the save-format callback identifier
  table were not altered.
- Removed thirteen macros that were not referenced by any compiled source:
  `MAXTHINGS`, `NUMTHINGS`, `ISMULT`, `CALLABLE`, `HEALTIME`, `STPOS`,
  `VS_PARALYZATION`, `VS_DEATH`, `VS_PETRIFICATION`, `VS_BREATH`, `ISBLIND`,
  `ISCANC`, and `ISSLOW`. Gameplay constants that are in use were retained.
- Removed redundant `<curses.h>` includes from 23 `.c` files. `rogue.h`
  remains the single source of public curses definitions for these files.
  The original source file layout was otherwise kept.
- Scoped the options table to `options.c` with `static`; it was not consumed
  outside that translation unit or saved as an address.
- Unified five signal handlers into a single `on_signal(int)` that **only**
  updates `volatile sig_atomic_t` flags; all terminal handling stays in
  ordinary control flow. Factored out repeated SIGHUP/SIGTERM reporting and
  termination without changing exit codes or automatic-save behavior.
- Added `-Wmissing-prototypes` to the default warning set. Neither C11 mode
  nor the existing source/link dependencies were changed.

**Compatibility:** The game rules, 80×24 windows, `R34SAV01` save schema,
`~/rogue.save` default save, `~/.rogue34.scr` score filename, score encoding,
RNG and tombstone order were not modified. No migration step was introduced.
`save.c`'s serialized fields and callback table were unchanged; its only
source edit was the redundant curses-header removal.

**Validation (Linux only):** Strict Clang 17 and GCC 14 builds of all source
files passed with `-std=c11 -Wall -Wextra -Wpedantic -Wmissing-prototypes
-Werror`, using a **test-only** curses declaration shim and installed ncurses
runtime libraries because the Linux environment lacks development headers.
A PTY regression suite passed 80×24 resize recovery, suspended resume after
undersizing, interactive-shell resume after undersizing, SIGTERM during the
size wait, SIGINT quit/cancel plus resize, and normal quit. A separate PTY
script passed inventory/options display and four consecutive game sessions
(new game followed by three restores, with resaving, ending in normal quit)
and `-s` score listing. Selected PTY tests with Clang ASan/UBSan also passed
(`detect_leaks=0`). This does **not** verify full-playthrough correctness.
No test-only code, headers, binaries, or object files are included in the ZIP.

### Stage 12: combat / inventory memory safety and boundary checks

- The monster runner iterator now remembers the next linked-list node before
  `do_chase()` calls `attack()`: leprechauns and nymphs can remove themselves
  mid-turn. `attack()` avoids its regeneration access after self-removal.
- When a killed monster drops its pack, `fight()` saves the next inventory
  node before `detach()` clears links and `fall()` possibly frees the node.
  This fixes the one-item-only drop behavior for multiple carried objects.
- `fight()` and `runto()` avoid null dereferences if map glyphs and the
  monster list become inconsistent. `remove_monster()` tolerates a missing
  list entry as a defensive measure.
- Thrown equipped weapons are unequipped, and a successful projectile hit
  frees the consumed item. Throwing a non-weapon whose landing square is
  blocked no longer indexes the weapon-name table out of range.
- Aborted drops on occupied tiles keep equipment pointers intact. A scare
  scroll that vanishes when dropped releases its detached object node.
- Confused missile directions reject `(0,0)`, which could otherwise hang the
  projectile flight loop at the player's original position.
- A stale trap glyph without matching `traps[]` metadata returns `-1` from
  `trap_at()`; all callers check that value before indexing the array.

**Validation (Linux only):** Clang 17 and GCC 14 compiled the full program
with `-std=c11 -Wall -Wextra -Wpedantic -Wmissing-prototypes -Werror`
without warnings using a **test-only** curses declarations shim and the
installed ncurses runtime. An ASan/UBSan standalone driver exercised two
consecutive leprechaun self-removals during monster traversal; a nymph's
item theft/self-removal; a killed monster dropping two carried objects;
a missing trap record; dropping onto an occupied tile; discarding a used
scare scroll; throwing an equipped weapon that hits a monster; and a
confused throw with an initially stationary random direction (seed 6).
The test passed without sanitizer diagnostics (`detect_leaks=0`).

Additional PTY regressions passed four successive game sessions with save /
restore, inventory/options and score listing; repeated window resize,
SIGTERM during a too-small window and SIGINT cancel/resize. These checks
are scoped, do not verify every monster or item effect, and do not cover exhaustive gameplay. Test drivers, the fake header, binaries and object
files are deliberately excluded from the distribution archive.

**Compatibility:** No changes to `save.c` or `rip.c` contents, save/score
encoding, file paths, default game rules or 80×24 terminal policy.

### Stage 13: bounded floor placement, connected dungeon testing, wandering monsters

**Observed nontermination:** A deterministic level-generation driver with the
original 15-bit RNG repeatedly stalled at depth 1 with initial seed `142705`.
Instrumenting `new_level()` isolated the issue to the unbounded staircase-floor
rejection loop, *after* `do_rooms()`, `do_passages()` and `put_things()` completed.
Empty floor tiles still existed: this was not exhaustion of the level, but an
unfortunate cycle in the small-state RNG and selection procedure. `rooms.c`
now defines `rogue_find_floor()` to preserve up to 256 normal rejection trials
and use a bounded room-by-room floor scan as a fallback. The same helper is
used for the staircase, traps, floor objects, amulet, hero, room monsters and
teleportation; a per-room-only option prevents ordinary room monsters from
spawning outside their assigned room. If there is no floor, extra objects or
traps are skipped. When the last floor is consumed after trap placement, one
trap is reclaimed to put the player on a valid floor. The normal generation
and RNG sequence are unchanged when the usual random trial succeeds.

**Monster occupancy:** `wanderer()` formerly inspected only `stdscr`. Active
monsters reside in `mw`, so another monster could be placed onto the same tile.
It now uses `winat()` and a bounded rejection/fallback search, skipping the
hero's room and any cell occupied by a monster. If no destination exists, the
spawn is skipped. `chase()` explicitly excludes out-of-bounds candidate
coordinates before invoking curses. Monster-owned object lists are freed
before removing the old floor's monsters during a level transition. The
teleport return value is based on the room actually reached, and scroll
identification checks it before indexing `rooms[]`.

**Connectivity test:** All 15 historical `conn_data` strings were checked to
span the full 3×3 room graph. Their maximum number of exits per room is four,
within the `r_exit[4]` array. A standalone test driver (not distributed)
called `new_level()` 1,050 times across depths 1, 3, 10, 26, 45, 75 and 300
(150 initial seeds per depth); a four-direction breadth-first search from
the hero found **no unreachable walkable tiles**, allowing secret doors as
potentially traversable cells. Additional assertions covered a fully occupied
preferred room, a saturated wandering-monster population without coordinate
overlap, and the decoder's maximum accepted depth of 1,000,000.

**Portability checks:** Strict C11 Clang and GCC builds passed on Linux without
warnings using a test-only curses header shim linked against the installed
ncurses runtime. The 1,050-level driver and targeted monster/depth cases ran
under ASan/UBSan (`detect_leaks=0`); previously verified save/restore and
terminal PTY scripts remained functional. The test-only declarations, scripts
and binaries are excluded from the release archive. No full playthrough was performed in these Linux tests. Existing score and save files are not migrated
or modified by any new feature; the `R34SAV01` binary schema is unchanged.

### Stage 14: potion/scroll interactions, cursed equipment and inventory validity

- `wizard.c` (`eat`): saved `o_which` before `discard(item)` and removed the
  comparison involving the freed object. The unpleasant-food experience
  bonus remains the same, and the hunger chain restarts as before. Separate
  pending effects continue to function.
- `wizard.c` (`wield`): **correction to stage 14**: the original Rogue 3.4
  allows a player to hold non-weapons, selected directly by inventory letter.
  The attempted type restriction was reverted; the item picker deliberately
  filters only the optional `*` inventory display.
- `potions.c` (`P_CONFUSE`): moved `on(player, ISHUH)` out of the impossible
  branch guarded by `off(player, ISHUH)`; the second potion lengthens the
  existing fuse rather than wasting the dosage without extending the state.
- `potions.c` (`S_HOLD`/`S_CREATE`): bounded all candidate coordinates to
  the fixed playable 80×24 interior before reading curses windows; the
  original in-range behavior is left unchanged.
- `wizard.c` (`create_obj`): validated item types and per-type indices,
  rejected creation when inventory is full, used initialized item fields,
  maintained `inpack` and preserved the curse bit on wizard-created weapons
  after their normal initializer. Wizard-created armor and weapons are now
  usable by the same save/restore schema as ordinary inventory objects.
- `main.c`: checks the `snprintf()` return for the default save path and
  refuses an unrepresentably long `HOME` path rather than silently writing
  to a truncated pathname. This also eliminated a GCC `-Wformat-truncation`
  issue under `-Werror`.

**Functional checks (Linux):** Clang and GCC passed
`-std=c11 -Wall -Wextra -Wpedantic -Wmissing-prototypes -Werror` with
zero warnings using a non-distributed curses API shim. A custom ASan/UBSan
harness confirmed food freeing with experience adjustment, hunger fuse
reset preserving an unrelated timer, non-weapon wielding, potion
confusion timer extension and expiry, repeated haste/exhaustion, cursed
armor/weapon removal and weapon enchantment, Wizard item-creation input
validation, valid pack count and curses bounds for hold/create scrolls.
`ASAN_OPTIONS=detect_leaks=0` was used. Four consecutive save/restore
sessions, normal score listing, Ctrl-Z/fg, shell escape, terminal resize
and signal-path PTY regressions all passed. A save produced from stage 13
was restored by stage 14 and consumed successfully. The prior save
schema and score code are unchanged.

### Stage 14 correction: preserve the original non-weapon wield rule

Inspection of the unmodified reconstructed 3.4 `wizard.c` confirmed that
`get_item("wield", WEAPON)` does **not** forbid selecting food, potions,
scrolls, the amulet, or armor by letter. The stage-14 restriction was an
unintended rule change and has been removed.

- `wizard.c`: `wield()` now retains the historical untyped selection.
  Existing curse restrictions on switching the held item remain in effect.
- `potions.c`: weapon enchantment continues to clear the item's curse and
  increase a bonus field even when the held item is not a weapon, as it did
  historically. The message no longer indexes `w_names` with a non-weapon
  subtype; the weapon-specific message is unchanged.
- `fight.c`: the missile launcher-match condition now also checks that the
  held item really is a weapon. Food variant 2 can no longer masquerade as
  a bow simply because both use numeric subtype 2.
- `save.c`: restore validates the held item by membership in the decoded
  inventory, not by an incorrect `WEAPON` requirement. Worn armor must still
  be armor. Both pointers may validly refer to the same armor object.
- `R34SAV01` file layout, save paths and score processing are unchanged.
  No legacy-save conversion or deletion is performed.

**Linux validation:** strict Clang and GCC C11 builds with `-Werror` passed
without diagnostics using the preexisting test-only ncurses header shim.
AddressSanitizer/UBSan targeted tests covered food/potion/scroll/armor
wielding, cursed-item behavior, enchanted held food and high-index scrolls,
consumption of a held item, and non-weapon missile launcher matching. Real
PTY game runs saved and restored held food and simultaneously worn-and-held
armor, retaining their inventory annotations; saved files were consumed on
successful restore. Exhaustive gameplay testing was not performed. Test harnesses and build shims are not distributed.


### Stage 15: monster-owned inventory lifetime and enabled leak checks

**Reproduced defect:** a monster with a nonempty `t_pack` that disappeared
through `remove_monster()` left the carried linked-list nodes and item data
unreachable. A standalone test built against the stage-14a source failed its
tracked allocation count assertion after the monster was removed.

**Correction:** `fight.c` frees the disappearing monster's remaining `t_pack`
entries before detaching and discarding the monster. For a normally killed
monster, `fight()` first moves its carried items to the floor (or discards
those that cannot land), leaving the monster's pack empty. That behavior
is unchanged. `new_level()` already frees the nested inventories of old
monsters and remains unchanged.

**Linux validation:** a temporary harness with live curses windows tested
monster removal with a carried item and 500 consecutive `new_level()` calls.
Each level optionally attached a carried item to a generated monster, and
each iteration added and removed an active fuse and daemon. After explicit
cleanup the allocated-list counter returned to zero. Clang 17 ASan/UBSan
with LeakSanitizer enabled (`ASAN_OPTIONS=detect_leaks=1`) finished cleanly.
Strict C11 `-Wall -Wextra -Wpedantic -Wmissing-prototypes -Werror` builds
passed with Clang and GCC using a test-only ncurses header shim. Interactive
PTY regressions for terminal resizing, Ctrl-Z/fg, shell resume, and
save/restore passed. A full interactive long-play memory profile was not
verified in these Linux tests. No test binaries,
headers or temporary drivers are included in the release archive.

**Compatibility:** game rules, 80×24 display, `R34SAV01`, `~/rogue.save`,
`~/.rogue34.scr`, and original `w` behavior are unchanged.


### Stage 16: compound-state save/restore and item-effect consistency

Tested five combinations of held items (worn armor, food, scroll, potion,
none) against a compound state that included a monster carrying two items,
a running monster targeting a room's gold coordinate, several independent
BEFORE/AFTER fuses, active daemons, status flags, item identification data,
equipment aliases, curses screens and the random seed. Saving, restoring and
re-saving produced byte-identical `R34SAV01` bodies in all five scenarios;
tracked timed effects expired on their original schedule.

- `save.c`: reject a running monster whose restored `t_dest` is NULL (a
  checksum-valid save with this combination could otherwise crash chase).
  Reject the same incoherent internal state at save time. Non-running
  monsters may still have a NULL target as in the original game.
- `fight.c`: Rogue 3.4 allows any inventory item to be held with `w`.
  Starting non-weapons can have NULL `o_damage`/`o_hurldmg` pointers; the
  old `roll_em()` path dereferenced such pointers. When the selected
  damage string is NULL, use the same `0d0` value used for generated
  non-weapon items. No wielding restriction was added.
- `potions.c`: `S_CREATE` increments its eligible-cell counter inside
  `rnd(++appear)`. This must be a count-capable integer; C11 `_Bool`
  saturates at one and changes the candidate selection. Use `int`.

**Tests, Linux:** strict C11 builds with Clang and GCC, both with
`-Wall -Wextra -Wpedantic -Wmissing-prototypes -Werror`, completed with
no diagnostics using an external test-only curses header and the real
ncurses shared library. A dedicated compound-state harness passed all
five scenarios with ASan/UBSan (`detect_leaks=0` for the harness), including
roundtrip comparisons, nested packs, aliasing of held and worn armor,
state flags and correct expiry of three independent fuses. The corrupt
running-monster/no-target case was rejected. A fresh game saved and
restored normally over a PTY; a stage-15 save was also restored and
re-saved successfully. None of the test-only shims/harnesses/binaries are
included in the distribution ZIP.

**Unchanged:** `R34SAV01` format/field ordering, save/score paths,
original random generator and game rules (apart from the identified
C11-induced sampling defect), 80×24 rendering, and original `w` semantics.


### Stage 17: original-rules comparison and passage-running correction

The reconstructed Berkeley Rogue 3.4 source was compared directly with the
stage-16 port; this stage specifically audits gameplay-affecting changes
rather than adding new functionality. A behavioral regression was confirmed:
the original `look()` initialized its `passcount` counter on each call, whereas
stage 16 had lifted it into `rogue_passcount` to support serialization but did
not reset it. The accumulated value could cause premature termination of
running/door-stop movement. `command.c` now resets `rogue_passcount` at the
start of every `look()`, restoring the reference's per-call semantics. The
`R34SAV01` field remains present at its existing position; `save.c` and the
binary file layout have **not** been changed.

**Regression reproduction:** a Linux curses test harness set the counter to
9, with `door_stop` and `running` enabled and no nearby corridors. The
stage-16 source stopped running (`running=0`), whereas the corrected source
reset the counter and left running enabled (`running=1`). The test-only
harness is not shipped.

**Static original-data comparison:** the original and stage-17 sources have
identical tokens in the 26-monster attribute table, both identification
probability tables (`s_magic`, `p_magic`), both armor arrays (`a_class`,
`a_chances`) and the original weapon damage/launcher table (`init_dam`) and
weapon names. For seven initial RNG seeds (including the 32-bit boundaries),
a reference implementation and the unsigned C11 calculation matched for
1,000 consecutive generated values each. These tests compare specified
properties; they do not prove complete behavioral equivalence.

**Regression checks, Linux only:** Clang 17 and GCC 14 passed the complete
C11 `-Wall -Wextra -Wpedantic -Wmissing-prototypes -Werror` build with no
warnings, using a test-only curses header and actual ncurses runtime. A PTY
test passed a new game, inventory, save, restore/re-save and normal quit. A
save produced by the unmodified stage-16 source was restored and re-saved by
stage 17. See [the original-rules audit](#original-rules-audit) below for known and intended rule deviations, test
methods and known limitations.

**Unchanged:** `R34SAV01` version/field order, default save and score paths,
score encoding, original ability to wield non-weapons and 80×24 fixed game
area. No save or score migration is performed.


### Stage 18: RNG-cycle detection preserves original placement sequences

A second pass over the 1981 placement logic found a game-behavior regression
in the stage-13 anti-hang fix. `rooms.c`/`rogue_find_floor()` and
`monsters.c`/`wanderer()` abandoned rejection sampling after **256 attempts**.
This cap can terminate a *still productive* original random sequence:
a reachable location may first appear at attempt 257 or later. The fallback
then changes both the placement and subsequent random state.

Both routines now track the RNG state before each attempt. The original RNG's
next value depends only on the low 15 bits of `seed`. If an attempt begins
with a previously seen low-15-bit state, subsequent room/position draws are
deterministic and cannot discover a new location on the unchanged map.
**Only at that point** do the routines use the existing bounded, non-random
fallback search. This still prevents nontermination when original sampling
cannot reach a free position. It does not change the fallback occupancy
criteria, the underlying RNG, monster attributes, or dungeon geometry.

**Focused checks (Linux):**

- A test compiling the actual stage-17 and stage-18 floor-search functions
  with deterministic RNG/curses doubles demonstrated a free room cell first
  sampled on attempt 521: stage 17 fell back after 512 RNG draws, whereas
  stage 18 reached it through original sampling after 1,042 draws. An
  ordinary first-attempt placement used 2 draws in *both* versions.
- With a free cell unreachable by the sampled coordinate pairs, stage 18
  recognized a repeated RNG state after 32,768 draws and used the fallback;
  it did not spin forever.
- A second test compiled the actual `wanderer()` implementations. A valid
  spawn first sampled on attempt 795 consumed 768 draws in stage 17 before
  fallback but 2,385 draws through the original sequence in stage 18.
  An ordinary first-attempt spawn consumed 3 draws in both versions.
- Two test-only builds with fixed seeds were used to generate new games with
  each stage. For 12 seeds (0, 1, 17, 42, 1234, 4095, 8191, 16384,
  32767, 65535, 1234567, 4294967295), the resulting immediate-save
  `R34SAV01` files matched byte-for-byte, including full game state and RNG.
  The test binaries had identical `dnum` and user options for comparability.
- Clang 17 and GCC 14 compiled and linked all sources with strict C11
  warnings treated as errors; a Linux pseudo-terminal check passed the
  new game, inventory, `S`, `-r`, re-save and `Q` flow. The whole-game
  PTY regression also passed under ASan/UBSan with leak checking disabled
  for that terminal test. The focused RNG tests passed ASan/UBSan with
  leak checking enabled.

These tests do **not** demonstrate complete identity to the historical
executable. They also do not establish that *all*
possible RNG paths are equivalent; the intentional fallback is still used
when a cycle makes random placement impossible.

**Unchanged:** save format `R34SAV01`, `~/rogue.save`, `~/.rogue34.scr`,
80×24 screen, original ability to hold non-weapons, and ordinary combat.
No migration or conversion of existing saved data is performed.


### Stage 19: original-rules audit of combat, monster selection and potions

Original `roll_em()`, `randmonster()`, `randwand()` and seven selected potion
`switch` branches were compiled alongside their stage-18 equivalents using
isolated C test harnesses and shared deterministic PRNG and state helpers.
All 72,000 combat comparisons, 546,200 monster-choice comparisons,
96,000 center-map chase comparisons and 104,000 ordinary potion-effect
comparisons matched the relevant results, state and RNG consumption. Another 8,000 repeated-confusion cases were **intentionally
different** because the port correctly extends an existing fuse rather than
leaving it unchanged; see [the original-rules audit](#original-rules-audit) below for details and limitations.

There was no evidence from these tests for a further game-rule change. The
only gameplay-source edit is a standard-library compatibility replacement
in `fight.c`: `index(cp, '/')` → `strchr(cp, '/')`, preserving functionality.
Linux Clang 17 and GCC 14 strict-C11 `-Werror` builds completed with
zero warnings, using the test-only curses declarations and real ncurses
shared runtime. Linux PTY checks passed new-game/inventory/save/restore/quit
and a stage-18-save to stage-19-restore/re-save/re-restore sequence. ASan and
UBSan passed the target function-level tests and the PTY flow (leak detection
was disabled for the terminal portion). The test harnesses remain outside
the distributed ZIP.

The default score path `~/.rogue34.scr`, save path `~/rogue.save`, versioned
format `R34SAV01`, 80×24 dungeon and original non-weapon wielding rule are
unchanged. No existing-data conversion is added.


### Stage 20: extended PTY stability tests and command count overflow

Stage 20 retains all stage-19 gameplay and save data, including the 1981
non-weapon wielding rule. It adds a single input-validation fix to `command.c`:
a decimal command repetition prefix larger than `INT_MAX` previously caused
signed overflow in `count = count * 10 + digit`. The failure was reproduced
in the unmodified stage-19 game with Clang UndefinedBehaviorSanitizer. The
new code checks each digit before multiplying and, on overflow, consumes the
remaining prefix and its following command, resets the repeat state and
prints `Count is too large.` without performing a game turn for that command.
Input values from 0 through `INT_MAX` retain their exact original integer
values. Tests covered `0Q`, `12Q`, `2147483647Q`, `2147483648Q`, and a
1,000-digit prefix ending in `Q`. In all five cases the revised game
responded appropriately and exited normally; no sanitizer error occurred.

**Linux integration checks, with unmodified game rules:** a pseudo-terminal
runner executed 180 ordinary-game sessions. Random movements, searches,
repeated commands, saving, restoring and quitting were combined. The 90
ASan/UBSan sessions resulted in 64 successful saves/restores, 60 subsequent
normal quits and 30 in-game endings. Forty-five optimized-Clang sessions
resulted in 35 saves/restores, 34 quits and 11 in-game endings; 45 optimized
GCC sessions gave 33 saves/restores, 32 quits and 13 in-game endings.
Across all 180 starts: **132 saves/restores, 126 quits, 54 in-game endings,
zero PTY timeouts, zero test failures**. The in-game endings include both
pre-save and post-restore outcomes, so the 132, 126, and 54 counts are not
mutually exclusive. Sent keystrokes are not asserted to equal executed turns.

**Single-process long-run checks:** in a *separate, modified test binary*,
the initial maximum HP and HP were set to 20,000 to avoid premature death.
No changes were made to the command, timer, monster AI, or screen routines
apart from test-only turn accounting. Five ASan/UBSan sessions recorded
4,265, 3,575, 3,105, 2,537 and 3,629 actual completed AFTER-phase turns
(17,111 total). Two more sessions with LeakSanitizer enabled recorded 3,978
and 3,066 turns (7,044 total); no sanitizer report was found in the tested
output. These are artificial survival conditions; they do not demonstrate
natural 4,000-turn survival, full-game victory, or exhaustive leak freedom.
Test-only HP modifications, turn counters and harnesses are excluded from the
release ZIP.

**Compatibility and original-rule regression:** 12 independent stage-19
portable saves were resumed by stage 20, re-saved and resumed again before
quitting. Additional normal-game tests exercised food and armor being wielded
with the original `w` command across three save/restore loops. Two *test-only* builds with identical fixed seeds performed `6s`
followed by saving. For 12 seeds (0, 1, 17, 42, 1234, 4095, 8191, 16384,
32767, 65535, 1234567 and 2147483646), the full `R34SAV01` byte streams
matched stage 19 exactly. This confirms the tested ordinary sequence, not
all possible inputs or game situations. Clang 17 and GCC 14 completed strict
C11 `-Wall -Wextra -Wpedantic -Wmissing-prototypes -Werror` builds with zero
warnings (Linux, using a test-only curses header and actual ncurses library).

**Unchanged:** serialized `R34SAV01` layout and semantics, `~/rogue.save`,
`~/.rogue34.scr`, 80×24 fixed game grid, original 15-bit PRNG, item and
combat rules, original ability to hold any inventory item, score behavior and
end-of-game/tombstone display. No legacy-data migration is introduced.


### Stage 21: release-structure and macOS-portability audit

The stage-20 source was checked for build targets, OS-specific compile/link
flags, public curses calls, runtime file locations, original historical
documents, and packaging cleanliness. No gameplay functions, score reader or
writer, or `R34SAV01` serializer were changed in this stage.

- The default `Makefile` uses the system compiler, C11, and `-lcurses` on
  Darwin. Linux uses ncurses plus libcrypt. Mandatory C11/warning flags are
  separated from user-selected `CFLAGS` so `make CFLAGS=...` does not
  accidentally revert the source to an older language mode. Neither platform requires
  GNU autotools or a third-party curses dependency on a normal macOS SDK.
- Apple documents the public `resizeterm()` and `wresize()` interfaces
  used by the terminal-resize code.
- Kept `rogue.doc`, `rogue.6`, `rogue.cat`, and `rogue.r` unchanged as
  historical documents. Their obsolete file paths and commands should not
  be mistaken for current port behavior.
- Omitted the unused historical `src/:ctfix` text-editor script from this
  modern-port source package. It is present in the original reconstruction.
- Replaced the accumulated stage-by-stage notes in `README.md` with a short
  introduction, build/use instructions, current file paths and limitations.
  Retained the detailed history here and
  the original-rules comparison (now consolidated in the [original-rules audit](#original-rules-audit) below).
- Added a minimal `.gitignore` for build products and macOS debug symbols.

**Stage-21 verification (Linux):** Clang 17 and GCC 14 both compiled and
linked the full source with C11 warnings promoted to errors, against the
previously used test-only curses declaration header and system ncurses
runtime. The final Clang build also passed a pseudo-terminal smoke test for
new game, inventory, save, restore, normal quit, and `-s` score display.
Generated binaries and test headers are excluded from the ZIP.

**Known release limitation:** `rip.c` updates an existing scores file in
place under an advisory `flock` lock. This prevents concurrent writers from
interleaving but does not make score updates atomic against process crashes
or power loss. A partial write could leave the file invalid. No score-format
or locking change is attempted in this documentation/build stage; address
this separately if crash-durable score storage is required.

**Unchanged:** 80x24 game grid, scoring and filenames (`~/.rogue34.scr` and
`~/rogue.save`), `R34SAV01` serialization, RNG, item/monster rules, and the
original ability to wield non-weapon items. This is a packaging/documentation
stage.


## Stage 22 — macOS terminal portability audit (score verification skipped)

- Rechecked `resizeterm()` and `wresize()` against documented macOS curses
  interfaces.
- Added failure checks for `initscr()` and the three game-window allocations.
  On allocation failure the terminal is restored and an error is reported,
  instead of dereferencing a null `WINDOW *`. Partial allocations during
  save restoration are cleaned up as well.
- Changed input waiting from an indefinitely blocking `read()` to a short
  `poll()` loop, checking terminal dimensions and pending signals periodically.
  This covers the race when SIGWINCH arrives just before blocking input; an
  undersized terminal now triggers the warning without requiring a keypress.
  Normal input remains event driven, without consuming game turns during the
  wait. `poll()` is a standard POSIX interface, available on macOS.
- Corrected an ABI mismatch in the Linux-only test curses shim: the return
  type of `isendwin()` is `bool`, not `int`. The shim was used for Linux test
  compilation only and is not part of this release.
- Linux: Clang/GCC strict C11 `-Werror` and an ASan/UBSan build passed;
  tested save/restore/resave, inventory and options, shrinking/enlarging the
  terminal, SIGTERM, Ctrl-Z/SIGCONT and shell escape. Forced failures of
  each of the three game-window allocations exited cleanly. A six-case
  save created by the earlier implementation was restored and resaved.
- **Skipped all new score-file verification by request.** `rip.c`,
  `rogue_score_path()` and the score file format and name (`~/.rogue34.scr`)
  are unchanged. This stage also leaves `R34SAV01` serialization, core
  game rules, and the 80x24 internal dungeon untouched.

## Stage 23 — Apple Silicon ABI and UTF-8 filesystem path audit (2026-11-21)

The stage-22 macOS validation source was used as the baseline. No changes were
made to the scoring implementation or to the game rules and RNG. The
`R34SAV01` binary layout remains unchanged.

- `home` and `file_name` now have a 512-byte capacity instead of the
  historical 80-byte capacity. Before using HOME, the source checks its
  entire byte length and appends a slash only when necessary. The filename
  itself must be at most 511 bytes. Long values fail instead of silently
  truncating filesystem paths.
- The `ROGUEOPTS=file=...` and interactive `file` option preserve the
  filesystem bytes, including UTF-8, rather than passing them through
  `rogue_unctrl()` (which is only suitable for the single-byte game text).
  Leading tilde expansion is preserved. Invalid overlong ROGUEOPTS filenames
  produce a startup error; the interactive option retains the prior path.
- The 80-column options screen limits *display* of a long path rather than
  scrolling into the map; the stored filename keeps its full byte sequence.
- The temporary save filename is allocated with room for the `.XXXXXX`
  suffix after a 511-byte path. The restoration load-path buffer is aligned
  with the new filename capacity.
- Restoration explicitly checks `initscr()` for failure before accessing
  curses screen dimensions. If initialization fails, the save is retained.
- Compile-time assertions document the assumptions of eight-bit bytes and
  32-bit `int`/`unsigned int`. In particular, no `long`, pointer, or curses
  WINDOW memory representation is written directly to the save file.

Linux-only verification:

- Clang 17, GCC 14 with `-std=c11 -Wall -Wextra -Wpedantic
  -Wmissing-prototypes -Werror`: full builds succeeded with no diagnostics,
  using the corrected *test-only* curses declaration shim and shared ncurses
  runtime. The shim is excluded from the release.
- Three build variants (Clang, GCC, Clang + ASan/UBSan) each passed tests
  saving and restoring under a nested UTF-8 HOME of more than 80 bytes,
  via `ROGUEOPTS=file=~/保存🍎.sav`, and via explicit absolute Unicode paths.
  Oversized HOME/ROGUEOPTS settings were rejected without truncation.
- An interactive `o`/`file` command using `~/手動🍎.save` correctly saved into
  a Unicode HOME directory. The standard terminal overlay, window resize,
  Ctrl-Z, shell-return, and SIGTERM checks passed on the optimized build.
- A genuine stage-22 `R34SAV01` save was loaded and re-saved by stage 23,
  then reloaded and re-saved. The format version is unchanged.
- An injected `initscr()` failure during restore exited with an error and
  kept the original save. This failure-injection hook is not shipped.
- Clang 17 target predefinitions for `arm64-apple-macosx14.0` confirm
  `CHAR_BIT=8`, 32-bit `int`, 64-bit `long` and 64-bit pointers. These were
  source-level checks; native macOS compilation and operation were subsequently
  confirmed by the maintainer. Score-file validation was intentionally skipped.

---

## Original-rules audit

This section integrates the former original-rules comparison document.
The stage-by-stage notes above record when each change was made; the
tables and detailed comparisons below distinguish exact matches from
intentional bug fixes and remaining limitations.

### Scope and reference

Reference: the supplied `rogue3.4-master.zip`, a reconstruction of the
1981 Berkeley Rogue 3.4 executable. Port: the stage-16 source plus
the later corrections through stage 20. The reconstructed source, not a later Rogue release,
is the comparison baseline. This is a source-based audit with targeted runtime
regressions, not a statement of full binary or gameplay equivalence.

### Detected unintentional behavior change — corrected

**`command.c`, `look()` passage count**

The reference initialized `int passcount = 0` on **every invocation** of
`look()`. When the port introduced a versioned save format, this became the
global `rogue_passcount`, which stage 16 incremented but did not clear at
function entry. As a result, the passage count accumulated across turns and
could stop a running command prematurely when `door_stop` was enabled.

Stage 17 resets `rogue_passcount = 0` once at the top of `look()`. The field
remains serialized for compatibility with all earlier `R34SAV01` saves, but
its previous value is no longer carried into the next look calculation.
A focused source-level regression test demonstrated the failure before the
change and the original behavior after it.

### Data and algorithmic comparisons

| Gameplay component | Source comparison | Assessment |
| --- | --- | --- |
| 26 monster names, flags, attributes, hit points, damage strings | Same 210 initializer tokens | Match |
| Scroll effect probability table (`s_magic`) | Same 28 initializer tokens | Match |
| Potion effect probability table (`p_magic`) | Same 24 initializer tokens | Match |
| Armor class and armor generation chances | Both original arrays unchanged | Match |
| Weapon damage, projectile damage, launcher and flags (`init_dam`) | Same 52 initializer tokens | Match |
| Weapon names | Same 12 initializer tokens | Match |
| Core `RN` random calculation | Same low 15-bit transition and output, now with unsigned arithmetic | Equivalent for tested 32-bit seeds |
| Wielding (`w`) | Food, potions, scrolls and armor may be held as well as weapons | Original rule maintained |
| Automatic running / junction handling | Reset of passage count restored to per-look scope | Regression fixed |

The RNG comparison used 1,000 successive values from each of seven initial
seeds: 0, 1, 42, 32767, `0x7fffffff`, `0x80000000`, and `0xffffffff`.
These checks do not establish that all game actions consume RNG values in the
same order under every circumstance.

### Intentional changes that may affect observable play

| Area | Port behavior | Reason and impact |
| --- | --- | --- |
| Internal dungeon size | Fixed at 80×24, even in a larger terminal | Avoid level geometry depending on terminal size; differs from the historical dynamic sizing in larger terminals |
| Object, trap, stair, player and monster placement | Original random position sampling continues until its 15-bit RNG state repeats; only then is a bounded deterministic search used | Avoid nonterminating loops; randomly unreachable free squares can lead to fallback placements; the original random stream is preserved until a provable cycle |
| Wandering monsters | Prevent spawning on an existing monster | Correct an occupancy inconsistency; changes those rare outcomes |
| Create-monster scroll | Candidate counter is `int` rather than C11 `_Bool` | Restores count-based reservoir sampling, which `_Bool` would distort |
| Confused projectile direction | Reroll if both direction components are zero | Prevent an infinite loop; an additional random draw occurs in that exceptional case |
| Non-weapon melee attack with missing damage string | Uses `0d0` rather than dereferencing NULL | Preserves wielding but specifies previously undefined behavior; may affect damage/bonus interpretation |
| Held non-weapon and enchant-weapon scroll | Keeps the original enhancement and uncursing effect; displays a generic name | Prevent out-of-range weapon-name lookup; user-visible text differs in these cases |
| Save files and scores | New portable versioned formats; `~/rogue.save`, `~/.rogue34.scr` | OS/architecture portability; historical raw memory saves and score layout are not preserved |
| Shared-host restrictions and signal behavior | Historical load/time limits removed; modern terminal handling | OS compatibility, not a dungeon mechanics change |

Safety fixes also address invalid array indices, use-after-free, stale trap
data and dangling item pointers. They are not intended to rebalance ordinary
combat, item drops, monster movement or experience progression.

### Verification performed for this stage

- A pre/post correction harness reproduced the stale `rogue_passcount`
  failure (stage 16: `running=0`, stage 17: `running=1` with an empty
  passage neighborhood).
- Clang 17 and GCC 14 on Linux each passed a strict C11 build with warnings
  treated as errors. A temporary curses declaration header was required in
  the analysis environment; it is not distributed.
- Actual ncurses pseudo-terminal runs exercised new-game display, inventory,
  save, restore, re-save, and normal quit.
- A valid save generated by the unmodified stage-16 executable was restored
  and saved again by the stage-17 executable.
- No game source other than `command.c` was modified for the rule correction;
  the save layout, probability tables and RNG implementation are unchanged.

### Remaining validation

The full 1981 executable was not run as an oracle, and no exhaustive gameplay
or identical-seed trace across all actions was completed. In particular,
rare fallback placement scenarios can still differ from the original, by
design. Future rule comparisons should distinguish such targeted bug fixes
from features added by later Rogue versions.


### Stage 18: correction to random-placement fallback timing

The stage-13 safety implementation stopped original rejection sampling after
256 failed attempts. This was an **unintentional RNG-behavior difference**
when a valid square existed later in the same original random sequence.
Because the historical generator has only 15 effective state bits, detecting
a repeated *start-of-attempt* state is sufficient to prove the future
candidate sequence would repeat indefinitely. The new search routines in
`rooms.c` and `monsters.c` defer non-random fallback until this cycle occurs.
This preserves the original draw count and seed for every successful original
sampling sequence; a genuinely cyclic/no-result sequence still terminates.

Direct function-level regression tests demonstrate floor sampling first
succeeding at attempt 521 and wandering-monster sampling at attempt 795.
Normal first-attempt cases consumed exactly the same draws in both stages.
All 12 tested initial level-generation seeds produced identical portable
save bytes in the stage-17 and stage-18 controlled builds. The occupancy
check to prevent overlapping wandering monsters remains in place, so those
exceptional spawn outcomes may still differ from uncorrected historical code.

No changes were made to the monster AI, combat, item probability tables,
seed update formula, RNG API, curses screen dimensions or serialized format.
Only the two random-placement search loops and documentation were changed.
The direct test harnesses are not included in the distributed archive.

### Stage 19: action-level fixed-seed comparisons

Source reference: `rogue3.4-master.zip` provided with this project. The
reference function **bodies** below were extracted from the reconstruction,
then compiled in separate test harnesses with the port bodies and identical
mocked state/PRNG support. The reference's K&R declarations and obsolete
`index()` spellings in the *test harness only* were adapted to permit
modern compilation. These are source-function comparisons, **not** an
execution of the original 1981 binary or an end-to-end matched playthrough.

| Area | Conditions checked | Observed comparison |
| --- | --- | --- |
| `fight.c`: `roll_em()` | 72,000 cases: six unarmed/melee/missile/launcher/dice configurations at 12,000 fixed seeds | Matching hit result, defender HP, final PRNG seed and draw count |
| `monsters.c`: `randmonster()` and `randwand()` | 546,200 comparisons, levels 1–100 and many initial seeds | Matching monster letter, final seed and number of draws |
| `chase.c`: `chase()` | 96,000 conditions near the center of an 80×24 map, including confused/bat-like movement | Matching destination, flags, return value, final seed and draw count with shared mocked grid/movement helpers |
| `potions.c`: selected effect branches | 104,000 cases of confusion (first dose), poison, healing, strength, extra healing, haste and restore strength | Matching modeled HP/strength/timer/flag/message side effects, final seed and draw count |
| `potions.c`: confusion during an existing confusion effect | 8,000 cases separately classified | Expected difference: the port extends the timer and consumes a random value; the historical branch did nothing |

The chase test uses shared stubbed `rndmove()` and walkable-grid helpers.
It excludes edge-of-screen cases where the original code could read beyond
the map; it cannot prove the behavior of the complete AI/game loop.
The potion test compares the selected **case bodies** with shared simplified
helpers for timing and messages, not the complete `quaff()`/inventory UI.
The combat test intentionally excludes historical undefined behavior from
non-weapons that have NULL damage strings; the port's explicit `0d0` fallback
is retained to avoid crashes. The monster-selection comparison does not
cover `wanderer()` placement, already covered separately in stage 18, or
combat-driven chase movement. Monster damage and the 26-type table remain
unchanged.

**Code change:** `fight.c` substitutes ISO C `strchr()` for the last BSD
`index()` usage when parsing a slash-separated damage string. This is a
portable spelling of the same search and consumes no RNG. No probabilities,
monster AI policy, item effect, save fields or score rules were changed.

The standalone PTY regression verified a new game, inventory, saving,
restarting with `-r`, and ordinary quitting. A stage-18-generated save
was restored and re-saved by stage 19, and then restored once more.
Strict Clang 17 and GCC 14 C11 builds used `-Werror` and gave zero warnings.
Clang ASan/UBSan passed the selected component tests and the PTY flow;
the terminal flow disabled leak checking. These Linux tests used a
**test-only curses declaration header**, not Apple's SDK.

**Limitations:** fixed-seed component tests cannot establish all-conditions
identical gameplay. The intentional exceptions catalogued above and in
stages 17–18 remain.


### Stage 20: repeat-count overflow and ordinary-game equivalence

The stage-20 change to `command.c` rejects **unrepresentably large decimal
prefixes** instead of relying on signed overflow. It is an input validation
fix, not a change to the original meaning of representable numeric prefixes.
For normal inputs, a 12-seed test compared the exact portable save bytes
after `6s` between stage 19 and stage 20. All 12 comparisons matched.
The direct comparison used test-only fixed-seed builds; the release has no
forced-seed or test-only HP changes. No combat, RNG, item, map or saved-field
logic was modified in stage 20.


## Stage 24 — visible projectile flight on modern terminals (2026-11-21)

The historical `missile()` redraws each visible projectile position without
waiting. On modern fast terminals the thrown item may be imperceptible.
A small POSIX `nanosleep()` wrapper, `rogue_visual_delay(milliseconds)`, is
added in `compat.c` and invoked in `weapons.c` **only after a visible
projectile step has been drawn**. The interval was initially 50 ms per square (changed to 30 ms in stage 25).
Interrupted sleeps are resumed for the remaining interval, without busy
waiting. The duration may be adjusted later based on actual terminal use.

This is a **presentation-only** change. It neither consumes random numbers
nor advances game turns; invisible steps, ordinary movement, monster AI,
combat, `jump` and `--More--` behavior remain unchanged. No new game option
or data field is added. Save `R34SAV01`, the score file and the historical
rule tables are unmodified. This is deliberately the first phase of the
animation improvements; continuous-running speed and brief transient messages
have not been changed.


**Stage-24 verification:** The delay function compiled under Clang 17 and
GCC 14 with C11, `-Wall -Wextra -Wpedantic -Werror` and ASan/UBSan in an
isolated test. The original 50 ms call took approximately 50.1 ms normally and
when interrupted by a signal. The changed `weapons.c` and `compat.c`
passed strict C11 compilation with a temporary curses declaration header.
The local environment lacks ncurses development headers, so a complete
regular game build and visual check on macOS were not performed for this
specific change. The temporary header and the test harness are not included.


## Stage 25 — 30 ms projectile and continuous movement timing (2026-11-21)

The visual flight pause added in stage 24 is reduced from 50 ms to **30 ms**
per visible projectile square. A shared `ROGUE_VISUAL_STEP_DELAY_MS` constant
is used for projectile flight and newly animated continuous movement.

In `playit()`, the extra 30 ms is applied after drawing the intermediate
frame of an automatic run (uppercase direction keys, including `f` running)
or a numeric repeat of a movement direction (`hjklyubn`). This keeps the
historical order of `look()`, `status()`, drawing, and `command()` intact.
It does **not** delay ordinary single-step movement or repeated non-movement
commands such as search. When `jump` is enabled, intermediate redraws and
visual pauses are both suppressed exactly as before. Other message timing,
monster AI, combat, random number consumption, saving and scores are unchanged.
The new delay is only a presentation feature; it adds neither game turns nor
persisted fields.

**Stage-25 verification (Linux):** Strict C11 Clang/GCC compilation and
linking succeeded with zero warnings treated as errors. This environment
used a **test-only curses declaration header** and the system ncurses
runtime because ncurses development headers were not installed. An isolated
ASan/UBSan test of the actual `playit()` loop passed nine conditions:
normal single movement, automatic running, numbered movement, repeated
search/throwing, and each relevant `jump` setting. The POSIX delay helper
was measured at approximately 30.1 ms, including interruption by a signal,
with Clang and GCC. A PTY game test covered starting, moving, quitting,
saving, and restoring. A fixed-seed test using identical commands compared
the saved game bytes from the stage-24 and stage-25 builds; both saved data
blobs were byte-for-byte equal. The fixed seed was injected in **test-only**
builds and is not present in the distributed source. macOS Terminal.app
visual pacing after this change remains to be checked by the maintainer;
Linux measurements do not guarantee exact 30 ms frame pacing on every OS.


## Stage 26 — magic effects and transient-message timing audit (2026-11-21)

This is an **audit only**. After comparing the stage-25 implementation with
its reconstructed Rogue 3.4 reference, no additional artificial sleep was
introduced in potions, scrolls, generic messages, or monster actions.

| Display path | Observed control flow | Timing decision |
| --- | --- | --- |
| Potion of monster detection (`P_MFIND`) | `show_win(hw, ...)` draws the detection overlay, then `wait_for(' ')` | Keep the existing space-key wait; no delay |
| Potion of magic detection (`P_TFIND`) | When magic is present, `show_win(hw, ...)` waits for space | Keep the existing space-key wait; no delay |
| Scroll of gold detection (`S_GFIND`) | When gold is present, `show_win(hw, ...)` waits for space | Keep the existing space-key wait; no delay |
| Scroll of magic mapping (`S_MAP`) | Copies the map into the game window; the result remains visible in later redraws | Do not add a temporary pause |
| Scroll of identification (`S_IDENT`) | Calls `whatis()` for item selection | Interaction itself waits for input |
| Lighting, enchanting, monster confusion, potion effects | Describe the effect with `msg(...)`; these are *text* effects rather than animated frames | Keep ordinary message handling |
| Consecutive messages | `msg()` draws `--More--` and waits for space before replacing a pending message | Keep the existing explicit wait |
| Ordinary messages | `command()` reads the next key before clearing the previous message | Do not add a universal text delay |
| Projectile flight / continuous movement | Stage-25 animation paths | Retain 30 ms per visible projectile square / displayed intermediate movement square; `jump` skips intermediate movement redraw/delay |

**Specific note:** Messages such as "The corridor glows and then fades"
and "Your armor glows faintly for a moment" describe an event; Rogue 3.4
does not implement a separate animated flash for these actions. Adding a
150–300 ms sleep after such text would generally make an already-readable
message linger without adding a visible animation. A new animation would
be an intentional presentation enhancement, not a correction needed for
message readability.

**Verification:** Read the complete potion/scroll effect dispatch in
`potions.c`, the `msg()`, `wait_for()` and `show_win()` paths in `io.c`,
the command input/clear sequence in `command.c`, and the display loop in
`main.c`; compared the waiting behavior with the reconstructed original
`io.c`/`potions.c`. A Linux PTY smoke test verified that a normal `v`
message remained present pending another command, that the full help overlay
blocked until space, and that quitting returned normally (exit status 0).
Clang and GCC C11 compilation/link tests passed with no compiler warnings,
using a **test-only** curses declaration header plus the system ncurses and
tinfo runtime libraries. This is **not** a substitute for a regular Linux
build using `libncurses-dev`, or an actual potion/scroll playthrough on a
Mac terminal.

**Source and compatibility:** No `.c`, `.h`, `Makefile`, `README.md`, save
format, or score implementation was changed. Only this `PORTING.md` audit
entry was added. The established 30 ms delays and original `--More--`
behavior are unchanged.


## Stage 27 — startup and successful-save notices (2026-11-21)

The greeting printed by `main.c` is flushed to the terminal and held for
**500 ms** before `initscr()` switches to the game screen. This covers both
normal and wizard greeting text. No delay is added when restoring a game.

The reconstructed 1981 `save.c` printed `File name: ...` after the save
filename was confirmed. The portable save implementation had omitted this
notice. It is now restored **only after** `write_save()` succeeds. The
message is drawn on the existing curses game screen, held for **500 ms**,
and then the unchanged command loop ends the curses session. Save failure
remains an ordinary visible error message with no artificial pause.

Both new pauses use the existing `rogue_visual_delay()` POSIX sleep helper
and the shared `ROGUE_TEXT_NOTICE_DELAY_MS` constant. Projectile flight and
visible repeated movement still use `ROGUE_VISUAL_STEP_DELAY_MS` (30 ms).
No random number is drawn, no game turn passes, and no save, score, or
original game-rule data layout is changed. Normal messages and `--More--`
retain their existing behavior.

**Stage-27 verification (Linux):** GCC and Clang C11 builds with warnings
as errors linked successfully to the system ncurses/tinfo runtime using a
**test-only curses declaration header**; the source ZIP does not include
this header. In a pseudo-terminal with an 80x24 screen, the greeting
remained visible for 0.501 s before the curses screen appeared, and the
successful-save file-name notice remained visible for 0.500 s before
exit. The resulting save file was present. A save to a nonexistent
directory displayed the existing error message, did not display the
success notice, and allowed the player to continue and quit normally.
This does not replace a native macOS Terminal.app visual check for this
new timing behavior.


## Stage 28 — 1000 ms startup and save notices (2026-11-21)

The two user-facing text notices introduced in stage 27 now remain visible
for **1000 ms** instead of 500 ms: the greeting shown just before the
initial curses screen and the `File name: ...` message shown only after a
successful save. Both call sites continue to use the shared
`ROGUE_TEXT_NOTICE_DELAY_MS` constant; its value was changed from `500u`
to `1000u`. The earlier stage-27 timings above are retained as historical
verification results, not the current configuration.

Projectile flight and visible continuous movement still use the separate
`ROGUE_VISUAL_STEP_DELAY_MS` constant (30 ms). No save format, game rule,
random-number call, score handling, or unsuccessful-save behavior changed.
The timing has not been measured in a native macOS run for this revision.


## Post-stage-28 — historical message fidelity (2026-11-23)

Rogue 3.4 deliberately preserves original English text, including unusual
wording, punctuation and typographical mistakes.  This is separate from JEC's
translation and editorial practices. Compared against the reconstructed
`rogueforge/rogue3.4` source:

- `save.c`: the confirmation prompt is once again exactly
  `Save file ("%s")? ` (including the space at the end). The existing
  yes/no input acceptance, atomic write, bounded path, and successful-save
  `File name: %s` notice with the 1000 ms display hold remain unchanged.
  The original filename-entry path for an empty preset filename has not
  been restored; that is a separate interaction-compatibility question.
- `save.c`: removed the additional `Game restored.` message so restoring
  directly resumes the saved screen, as in the reconstructed original.
- `command.c`: a failed child `execl()` once again uses the original
  `perror("No shelly")` text (typo intentionally retained). New errors
  for `fork`, signal handling and privilege changes remain as safety and
  modern-OS diagnostics.
- `potions.c`: an enchanted *held non-weapon* uses the original
  `Your %s glows blue for a moment.` message whenever its `o_which`
  is a valid `w_names` index. The original code could read outside that
  array for certain non-weapons; the guard keeps this memory-safety fix,
  and an invalid index produces no substitute invented game message.
  Enchantment, uncursing and RNG consumption are unchanged.

No changes were made to the score file representation, tombstone year,
post-game Return-key prompts, 1000 ms startup/successful-save notices,
terminal handling, save-file format or gameplay rule data. The former
remain intentional portability/display differences to audit separately.

**Verification:** The three changed C files passed GCC and Clang C11 syntax
checks with a temporary, test-only minimal curses declaration header. A
source-to-source archive comparison confirmed that only those three C files
and this history document changed. The test header is not distributed. A
normal build and interactive terminal playthrough remain unverified in this
environment because the ncurses development headers are unavailable.


## Post-stage-28 — original save filename selection and 200 ms notice (2026-11-23)

The previous `save_game()` treated `n` as a cancellation and disallowed
interactive name selection. Restored the reconstructed 1981 `save.c` flow:

- When a preset save filename exists, `S` asks `Save file ("%s")? `;
  `y`/`Y` selects that name, `n`/`N` enters a new name, rather than quitting.
- When no filename is preset, prompt `File name: ` directly.
- After a file I/O failure, report the failure and permit entering another
  filename. Escape during filename entry cancels the save, as before.
  Interactive entry remains bounded (original 80-byte input buffer)
  and uses the existing safe filename setter.
- The historical `File name: %s` notice is now displayed **before** the
  atomic save operation, not after successful writing. It is refreshed
  immediately. A monotonic clock measures elapsed time since it appeared;
  only any remaining time to reach 200 ms is slept. A slow write incurs
  no additional artificial pause. Failed attempts also hold the notice
  briefly before showing the error and offering another filename.
- The greeting retains its independent 1000 ms delay; game mechanics, RNG,
  portable save format (`R34SAV01`), integrity checks, atomic replacement,
  and save consumption on restore are unchanged.

Historical earlier stage entries describing the 1000 ms post-success save
notice remain as development history, not the current specification.

**Verification of this revision (Linux, 2026-11-23):** GCC and Clang C11
syntax checks with warnings as errors passed for `save.c`. A full GCC build
and link succeeded using a **test-only curses declaration header** and the
system ncurses/tinfo and crypt runtime libraries (the system's real curses
development headers are unavailable). PTY interaction tests covered `S,y`,
`S,n` with a new name, no preset filename, failed save followed by error
acknowledgement and a new name, Escape cancellation, and a save/restore
round trip. In the four successful save cases the terminal's filename-notice
to-exit interval measured approximately 0.201–0.202 s. A separate TEST-ONLY
build injecting a 350 ms delay after the filename notice completed after
approximately 0.353 s, demonstrating that the minimum is total elapsed time,
not an additional post-save delay. The fake header and injected delay are
**not** part of the distributed source; native macOS testing remains to be
done.


## Post-stage-28 — original save directory and score basename (2026-11-23)

For the historical-behavior-focused Rogue 3.4 port (separate from JEC):

- Restored the original effective default save filename `rogue.save` **relative
  to the game's current working directory**.  The reconstructed 1981
  `main.c` first copies `home` but immediately overwrites that path with
  `rogue.save`.  Explicit filenames from `ROGUEOPTS`, in-game `o` options,
  `S` filename entry, and named restore arguments remain supported.
  `./rogue -r` now refers to the current directory's `rogue.save` by default.
- Restored the original score **basename** `.rogue_roll`, while deliberately
  keeping the modern per-user score directory (`~/.rogue_roll`) rather than
  the original shared `/usr/public/.rogue_roll`.  Multi-user score sharing
  is deferred; no build-time filename options were added.
- The score-path formatter now rejects an overlong HOME path explicitly
  instead of silently writing the score file to the working directory.
- Historical mentions of `~/rogue.save` and `~/.rogue34.scr` in earlier
  stages above are retained as a record of those earlier implementation
  states, not as descriptions of the current defaults.

The save-file format `R34SAV01`, atomic save/restore, score serialization,
file locking, original save-name selection, 200 ms save notice and all game
rules remain unchanged.  Historical `rogue.doc`, `rogue.6` and `rogue.r`
are unmodified.


## Post-stage-28 — aggravate-monsters scroll null destination safety (2026-11-26)

In `src/potions.c`, `S_AGGR` originally marked every current-level monster
`ISRUN` without necessarily assigning a destination. A newly generated
monster may have `t_dest == NULL`, but `do_chase()` unconditionally
dereferences `t_dest`; reading the scroll could therefore crash. The
1981 reconstructed source contains this same defect.

Before setting `ISRUN`, the scroll now sets `t_dest = &hero` **only when**
`t_dest` was null. Existing destinations and every other monster flag are
unchanged. The original scroll message, identification, effect scope,
and RNG usage remain unchanged. This is a memory-safety fix, not a change
to the original game rules.

**Verification (2026-11-26):** GCC and Clang C11 `-Wall -Wextra -Wpedantic -Werror`
syntax checks passed for `potions.c` using test-only curses declarations.
A standalone C test executing the actual `S_AGGR` monster loop passed
under both compilers with undefined-behavior sanitization: a null
destination is initialized, an existing destination is preserved, other
flags remain intact, and an empty list or repeated use is safe. The
test scaffolding is not distributed. A full native build and interactive
gameplay test have not been performed in this environment.

## Post-stage-28 — Leprechaun death in a corridor (2026-11-26)

Rogue 3.4's `fight.c` dereferenced `roomin(mp)` without checking its
result when a leprechaun was defeated. `roomin()` returns `NULL` for
a corridor square, so killing a leprechaun there could crash.

Following the narrow guard already present in the reconstructed Rogue
3.6 `fight.c`, the 3.4 port now tests the room pointer and skips **only
the leprechaun's gold-creation case** when it is null. Monster defeat,
experience credit and the shared monster removal code still proceed.
In-room gold placement and quantity, saving-throw logic, message text
and RNG consumption remain identical to this port's prior version.
No corridor-gold subsystem was added: gold in Rogue 3.4 belongs to rooms.
The existing safety changes and historic `rogue.doc`, `rogue.6`,
`rogue.r` are untouched.

Verification: the change was checked against the reconstructed Rogue
3.6 implementation; syntax checks and targeted corridor-versus-room
behavior tests were performed. Native macOS gameplay is still untested.
