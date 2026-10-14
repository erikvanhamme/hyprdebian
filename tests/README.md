# Tests

Every directory here is a test suite for one part of hyprdebian. The tests use Python `unittest`, so they need no packages beyond those of the part they test.

Run all suites, or one:
```
tests/run.sh
tests/run.sh hd-backup
```

Measure coverage (needs `python3-coverage`) for the suites that have a `.coveragerc`:
```
tests/run.sh --coverage
tests/run.sh --coverage hd-backup
```

This prints a report per suite, with the lines and branches that did not run, and writes an HTML report to `tests/<suite>/htmlcov/index.html`. Programs a test runs as a separate process are measured too.

Run one test file or one test from inside a suite directory:
```
cd tests/hd-backup
python3 -m unittest test_scan
python3 -m unittest test_scan.TestScan.test_recreated_file_is_on_disk_and_old_record_is_not
```

## hd-backup
Tests for the backup tool `deploy/usr/local/bin/hd-backup` and `src/blockwrite.c`. They need `python3-colorama`, `python3-tabulate` and `tar`; the `blockwrite` tests also need `gcc`. They run as a normal user, and only touch temporary directories. A few tests are skipped as root, because root can read files without read permission.

| File | Tests |
| --- | --- |
| `test_units.py` | Helper functions without a database: tar sizes, exclusions, hashes, file attributes, matching. |
| `test_accounting.py` | Which items count as written when a carrier runs full, also against real tar output. |
| `test_scan.py` | Scanning filesets on an in-memory database: new, changed, deleted and recreated items, special files, exclusions. |
| `test_write.py` | The parts of a write that do not run tar (delete list, file list, carrier choice, database update), and the backup, carrier, summary and manifest commands. |
| `test_commands.py` | Every command run as a program, and its exit status. |
| `test_backup_restore.py` | Backups written to a directory and to a stub tape drive, restored with tar and the delete list, and compared with the live files. |
| `test_blockwrite.py` | `blockwrite`, compiled from source. |

`helpers.py` loads `hd-backup` for the tests. The script handles its command line at the top level, so it cannot be imported: the helper runs the part before the `# Main` section, which defines the functions.

The tape tests take about 5 seconds each, because a tape write waits 5 seconds before it rewinds.
