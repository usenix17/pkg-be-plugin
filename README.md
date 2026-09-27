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
- `/usr/local/lib/pkg/be.so` -- the plugin shared object
- `/usr/local/share/man/man8/pkg-be-plugin.8.gz` -- the manual page

### Installing from a release package

Each [release](https://github.com/usenix17/pkg-be-plugin/releases) ships a
prebuilt `.pkg` and a detached signature. Verify against the repository's
public key, then install:

```sh
openssl dgst -sha256 -verify pkg-be-plugin.pub \
    -signature pkg-be-plugin-1.0.1.pkg.sig pkg-be-plugin-1.0.1.pkg
pkg add ./pkg-be-plugin-1.0.1.pkg
```

pkg(8) loads only plugins that are explicitly enabled. Add the plugin to
`/usr/local/etc/pkg.conf`:

```ucl
PLUGINS [ "be" ];
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
| `BE_PLUGIN_REPOSITORIES` | string | `` | Comma-separated list of repository names that trigger BE creation. Empty = all transactions. Use `local` to match packages installed via `pkg add` |

Duration values accept a bare integer or an integer with a suffix:
`d` (days), `h` (hours), `m` (minutes), `s` (seconds).  `0` disables the restriction.

Example `be.conf` scoped to base system updates only:

```ucl
BE_PLUGIN_ENABLED = true;
BE_PLUGIN_KEEP = 10;
BE_PLUGIN_MIN_AGE = "14d";
BE_PLUGIN_STRICT = false;
BE_PLUGIN_SKIP_TRANSACTIONS = "deinstall";
BE_PLUGIN_REPOSITORIES = "FreeBSD-base";
```

To create BEs for all transactions (the default), omit `BE_PLUGIN_REPOSITORIES`
or leave it empty.

## Rolling back

Boot environment names are logged to syslog on creation.  To find and activate
the environment created before the last transaction:

```sh
# Find the name
grep "pkg-be-plugin: created" /var/log/messages | tail -1

# Activate it for next boot
bectl activate pre-pkg-20260513T142301

# Reboot
reboot
```

Logging honours the global `SYSLOG` option of pkg.conf(5); if you have
disabled it, the plugin writes no syslog entries. In that case list the
environments by creation time instead:

```sh
bectl list -c creation
```

## Running the tests

```sh
cd tests && make
```

Tests cover `parse_duration()`, `parse_skip_transactions()`, the pruning sort
order, and the prefix-match logic.  They have no ZFS or libbe dependency.

## License

BSD 2-Clause.  See individual source files for the full license text.
