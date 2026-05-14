# pkg-be-plugin 1.0.0 — 2026-05-14

Initial release.

## What it does

pkg-be-plugin is a pkg(8) plugin for FreeBSD that automatically creates a ZFS
boot environment before each package install, upgrade, or deinstall transaction.
If a transaction leaves the system in a broken state, the pre-transaction
environment can be activated with bectl(8) and booted into for recovery.

Boot environments are created and pruned using libbe(3) directly, with no
exec of bectl(8) or zfs(8).

## Configuration

The plugin is configured via `/usr/local/etc/pkg/be.conf` (UCL). A default
configuration file is installed; all keys have compiled-in defaults so the
file can be left as-is.

| Key | Default | Description |
|-----|---------|-------------|
| `BE_PLUGIN_ENABLED` | `true` | Master switch |
| `BE_PLUGIN_KEEP` | `5` | Maximum matching BEs to retain |
| `BE_PLUGIN_NAME_PREFIX` | `pre-pkg` | Prefix for generated BE names |
| `BE_PLUGIN_MIN_AGE` | `7d` | Minimum age before a BE is eligible for pruning |
| `BE_PLUGIN_STRICT` | `false` | Abort the transaction if BE creation fails |
| `BE_PLUGIN_SKIP_TRANSACTIONS` | `` | Comma-separated list of transaction types to skip: `install`, `upgrade`, `deinstall` |

Duration values accept a bare integer (seconds) or a suffix: `d`, `h`, `m`, `s`.

## Syslog output

All events are logged to the `daemon` facility under the `pkg-be-plugin` ident:

- `LOG_NOTICE` — successful creation, successful pruning, deferred pruning notice
- `LOG_WARNING` — creation failures in non-strict mode; all pruning failures
- `LOG_ERR` — creation failure in strict mode (transaction aborted)

## Known limitations

- Requires the system to be booted from a ZFS boot environment. On UFS roots or
  inside jails without ZFS access, libbe_init(3) fails; in non-strict mode this
  is logged and the transaction proceeds.
- Pruning uses the ZFS creation timestamp reported by libbe(3) to determine age
  ordering. Clocks that are adjusted between transactions can affect sort order.
- The active and next-boot boot environments are protected by libbe(3) itself;
  be_destroy(3) refuses to destroy them and the failure is logged.

## Bugs fixed during development

The following issues were found and resolved during integration testing before
this release:

- **Segfault on config parse failure**: `openlog(3)` stores its ident as a raw
  pointer into the plugin's text segment. Calling it before any error-return
  path in `pkg_plugin_init` caused a use-after-free when pkg skipped shutdown
  and dlclose'd the library. Fixed by moving `openlog` to after all error paths.
- **Pruner always matched zero BEs**: libbe(3) returns the `creation` property
  as a decimal Unix epoch string (`DATA_TYPE_STRING`), not a `uint64`. The
  initial code called `nvlist_lookup_uint64`, which always failed silently.
  Fixed by switching to `nvlist_lookup_string` and parsing with `strtoll(3)`.
- **Concurrent libbe handles**: the hook held an open libbe handle while calling
  into prune_old_bes(), which opened a second one. The second handle was
  unreliable. Fixed by closing the hook's handle before calling the pruner.
