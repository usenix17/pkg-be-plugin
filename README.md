# pkg-be-plugin

A [pkg(8)](https://man.freebsd.org/pkg/8) plugin for FreeBSD that automatically
creates a ZFS boot environment before each package install, upgrade, or
deinstall transaction.  If a transaction breaks the system, boot into the
pre-transaction environment to recover.

Boot environments are created and pruned using
[libbe(3)](https://man.freebsd.org/libbe/3) directly — no `bectl` or `zfs`
subprocesses.

## Requirements

- FreeBSD with a ZFS boot environment (UFS root is not supported)
- `pkg(8)` with plugin support
- `libbe` (part of the base system since FreeBSD 12)

## Building

```sh
make
```

## Installing

```sh
make install
```

This installs:
- `/usr/local/lib/pkg/be.so` — the plugin shared object
- `/usr/local/man/man8/pkg-be-plugin.8` — the manual page

Enable the plugin in `/usr/local/etc/pkg.conf`:

```ucl
PLUGINS_CONF_DIR = "/usr/local/etc/pkg";
PLUGINS [ be ];
```

## Configuration

Configuration lives in `/usr/local/etc/pkg/be.conf` (UCL format).  A missing
file is not an error; all keys have compiled-in defaults.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `BE_PLUGIN_ENABLED` | bool | `true` | Master switch |
| `BE_PLUGIN_KEEP` | int | `5` | Max auto-created BEs to retain |
| `BE_PLUGIN_NAME_PREFIX` | string | `pre-pkg` | Prefix for generated BE names |
| `BE_PLUGIN_MIN_AGE` | duration | `7d` | Minimum age before a BE is eligible for pruning |
| `BE_PLUGIN_STRICT` | bool | `false` | Abort the transaction if BE creation fails |
| `BE_PLUGIN_SKIP_TRANSACTIONS` | string | `` | Comma-separated list of transaction types to skip: `install`, `upgrade`, `deinstall` |

Duration values accept a bare integer or an integer with a suffix:
`d` (days), `h` (hours), `m` (minutes), `s` (seconds).  `0` disables the restriction.

Example `be.conf`:

```ucl
BE_PLUGIN_ENABLED = true;
BE_PLUGIN_KEEP = 10;
BE_PLUGIN_MIN_AGE = "14d";
BE_PLUGIN_STRICT = false;
BE_PLUGIN_SKIP_TRANSACTIONS = "deinstall";
```

## Rolling back

Boot environment names are logged to syslog on creation.  To find and activate
the environment created before the last transaction:

```sh
# Find the name
grep "pkg-be-plugin: created" /var/log/messages | tail -1

# Activate it for next boot
bectl activate pre-pkg-20260513-142301

# Reboot
reboot
```

## Running the tests

```sh
cd tests && make
```

Tests cover `parse_duration()`, `parse_skip_transactions()`, the pruning sort
order, and the prefix-match logic.  They have no ZFS or libbe dependency.

## License

BSD 2-Clause.  See individual source files for the full license text.
