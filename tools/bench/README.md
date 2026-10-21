# Shell benchmarks

Startup time, memory and idle CPU for Basecamp shells, on a hidden X display (Xvfb `:101`, software rendering).

- `bench.sh <bundle> <label> [runs] [ENV=val...]`: per run, a fresh `--user-dir` copied from `$TEMPLATE`
  (default `$BENCH_DIR/template-empty`, an empty folder). Prints one TSV line per run:
  `label run t_window t_idle main_pss_kb main_rss_kb all_pss_kb all_rss_kb processes`.
  `t_idle` = all Basecamp processes under 3% CPU for 3 s. Run 1 starts with an empty QML cache.
- `idle.sh <bundle> <label> [ENV=val...]`: starts once, waits 20 s, prints CPU over 10 s per process and
  the busiest threads of the main process.

Variants used: stock (upstream base `5f8cca8`), `BASECAMP_LAYOUT=sidebar` (Default), `BASECAMP_LAYOUT=desktop`,
`BASECAMP_SHELL=basecampium` with a template whose `plugins/` has `basecampium.lgx` installed by `lgpm`.

Results: see issue #20 and `results-*.tsv`.
