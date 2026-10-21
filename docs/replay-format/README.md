# CS2KZ replay file format

`.replay` files are written by the CS2KZ Metamod plugin. They hold a player's movement tick by tick, plus the weapons,
jumpstats, timer events and raw user commands needed to play it back.

The format is versioned. The version is stored in the file's header, and it selects the layout of everything after it.

| Version | Page | Summary |
|---|---|---|
| 1 | [v1.md](v1.md) | Base format |
| 2 | [v2.md](v2.md) | Adds modern jump state to ticks |
| 3 | [v3.md](v3.md) | Moves the modern jump flags to their own bits |
| 4 | [v4.md](v4.md) | Smaller fixed subtick entries (36 move slots) |
| 5 | [v5.md](v5.md) | AA records gain start and end fractions |
| 6 | [v6.md](v6.md) | Events become length-prefixed protobuf messages |
| 7 | [v7.md](v7.md) | 64-bit section headers, multi-frame sections, packed subticks |

> **Version 7 is not released yet.** Its layout can still change before the first plugin release that writes it.
> Versions 1 to 6 are frozen.

Each version page lists, section by section, which layout that version uses. The layouts themselves are defined once in
[common.md](common.md):

- [Conventions](common.md#conventions)
- [File layout](common.md#file-layout) and the [replay header](common.md#replay-header)
- [Section containers](common.md#section-containers)
- [Tick encoding](common.md#tick-encoding)
- [Subtick entries](common.md#subtick-entries)
- [Weapon table](common.md#weapon-table)
- [Jump records](common.md#jump-records)
- [Events](common.md#events)
- [Command encoding](common.md#command-encoding)

The reference implementation is `src/kz/replays/compression.cpp` and `src/kz/replays/parsing.cpp`. Where these pages and
the code disagree, the code is authoritative.
