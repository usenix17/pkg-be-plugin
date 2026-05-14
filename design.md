# pkg-be-plugin: Architecture Design

FreeBSD pkg(8) plugin to auto-create ZFS boot environments before package
transactions using libbe(3) directly.

---

## Reference Summary (Step 1 findings)

### libbe API (`/usr/include/be.h`, `man libbe`)

**Handle lifecycle:**
```c
#include <be.h>                     /* links with -lbe */

libbe_handle_t *libbe_init(const char *be_root);  /* NULL → autodetect pool */
void            libbe_close(libbe_handle_t *);
```
`libbe_init(NULL)` returns NULL on failure (no errno set; the process's ZFS pool
is used). Every opened handle must be closed on every code path, including error
returns.

**Error reporting:**
```c
int         libbe_errno(libbe_handle_t *);
const char *libbe_error_description(libbe_handle_t *);
void        libbe_print_on_error(libbe_handle_t *, bool);   /* default: false */
```
We must call `libbe_print_on_error(hdl, false)` to suppress automatic stderr
output and use `libbe_error_description()` to feed messages to
`pkg_emit_error()` instead.

**BE creation:**
```c
int be_create(libbe_handle_t *, const char *be_name);
```
Creates a new BE by taking a recursive snapshot of the *currently booted* BE
and cloning it. Returns 0 on success, a `be_error_t` otherwise.

**BE enumeration (for pruning):**
```c
int  be_prop_list_alloc(nvlist_t **be_list);
int  be_get_bootenv_props(libbe_handle_t *, nvlist_t *be_list);
void be_prop_list_free(nvlist_t *be_list);
```
The resulting `nvlist_t` contains one `nvpair_t` per BE. Each pair's name is
the BE name; its value is an `nvlist_t` of properties. Relevant properties:

- `"name"` — BE name (always present)
- `"creation"` — `DATA_TYPE_UINT64`, Unix timestamp (may be absent)
- `"active"` — present and true if currently booted
- `"nextboot"` — present and true if active on next boot

Iteration pattern:
```c
nvpair_t *pair = NULL;
while ((pair = nvlist_next_nvpair(be_list, pair)) != NULL) {
    const char *be_name = nvpair_name(pair);
    nvlist_t   *props;
    nvpair_value_nvlist(pair, &props);
    uint64_t creation;
    nvlist_lookup_uint64(props, "creation", &creation);
}
```

**BE destruction:**
```c
typedef enum {
    BE_DESTROY_FORCE      = 1,
    BE_DESTROY_ORIGIN     = 2,
    BE_DESTROY_AUTOORIGIN = 4
} be_destroy_opt_t;

int be_destroy(libbe_handle_t *, const char *be_name, int flags);
```
We will use `BE_DESTROY_ORIGIN` to also remove the underlying snapshot.

**Name utilities:**
```c
int be_validate_name(libbe_handle_t * __unused, const char *); /* checks chars/length */
int be_exists(libbe_handle_t *, const char *);                  /* 0 = exists */
```

---

### pkg plugin API (`/usr/local/include/pkg.h`, pkg 2.7.5)

**Plugin entry points** (exported by the `.so`, confirmed from
`strings /usr/local/lib/libpkg.so`):
```c
int pkg_plugin_init(struct pkg_plugin *p);      /* called at load */
int pkg_plugin_shutdown(struct pkg_plugin *p);  /* called at unload */
```
Return `EPKG_OK` (0) or `EPKG_FATAL`.

**Metadata:**
```c
pkg_plugin_set(p, PKG_PLUGIN_NAME,    "be");
pkg_plugin_set(p, PKG_PLUGIN_DESC,    "Auto-create ZFS boot environments");
pkg_plugin_set(p, PKG_PLUGIN_VERSION, "1.0.0");
```

**Hook registration:**
```c
typedef int (*pkg_plugin_callback)(void *data, struct pkgdb *db);
int pkg_plugin_hook_register(struct pkg_plugin *p,
    pkg_plugin_hook_t hook, pkg_plugin_callback cb);
```
Available hooks we need:
```
PKG_PLUGIN_HOOK_PRE_INSTALL    = 1
PKG_PLUGIN_HOOK_PRE_DEINSTALL  = 3
PKG_PLUGIN_HOOK_PRE_UPGRADE    = 7
```
The `void *data` argument passed to the callback for all three of these is
`struct pkg_jobs *`. Transaction type can be distinguished with
`pkg_jobs_type(jobs)` → `PKG_JOBS_INSTALL`, `PKG_JOBS_DEINSTALL`, or
`PKG_JOBS_UPGRADE`.

**Error/notice reporting:**
```c
void pkg_emit_error(const char *fmt, ...);
void pkg_emit_notice(const char *fmt, ...);
void pkg_plugin_error(struct pkg_plugin *p, const char *fmt, ...);
void pkg_plugin_info(struct pkg_plugin *p, const char *fmt, ...);
```
We will use `pkg_plugin_error()` for user-visible error messages and
`pkg_plugin_info()` for notices. Both are prefixed with the plugin name by
libpkg.

**Config API** (UCL-based, reads from `PLUGINS_CONF_DIR/<name>.conf`):
```c
/* In pkg_plugin_init(), declare all keys before calling parse: */
pkg_plugin_conf_add(p, PKG_BOOL,   "BE_PLUGIN_ENABLED",          "true");
pkg_plugin_conf_add(p, PKG_INT,    "BE_PLUGIN_KEEP",              "5");
pkg_plugin_conf_add(p, PKG_STRING, "BE_PLUGIN_NAME_PREFIX",       "pre-pkg");
pkg_plugin_conf_add(p, PKG_STRING, "BE_PLUGIN_MIN_AGE",           "7d");
pkg_plugin_conf_add(p, PKG_BOOL,   "BE_PLUGIN_STRICT",            "false");
pkg_plugin_conf_add(p, PKG_STRING, "BE_PLUGIN_SKIP_TRANSACTIONS",  "");
pkg_plugin_parse(p);

/* Access values: */
const pkg_object *conf = pkg_plugin_conf(p);
const pkg_object *obj;
pkg_iter it = NULL;
while ((obj = pkg_object_iterate(conf, &it)) != NULL) {
    const char *key = pkg_object_key(obj);
    /* then pkg_object_string/bool/int(obj) */
}
```

**Return values from hooks:**
```c
EPKG_OK    = 0    /* continue the transaction */
EPKG_FATAL        /* abort the transaction (used in strict mode) */
```

---

### Build environment

- `/usr/lib/libbe.so` — libbe shared library (base system)
- `/usr/local/lib/libpkg.so` — libpkg (pkg 2.7.5)
- Plugin install dir: `/usr/local/lib/pkg/` (`pkg config PKG_PLUGINS_DIR`)
- Plugin config dir: `/usr/local/etc/pkg/` (`pkg config PLUGINS_CONF_DIR`)
- ATF: `kyua` at `/usr/bin/kyua`, `atf-c.h` at `/usr/include/atf-c.h` (base)

Plugins are unversioned `.so` files loaded via `dlopen()`. The filename must
match the plugin name in `pkg.conf`'s `PLUGINS` array (e.g., `PLUGINS [ "be" ]`
loads `be.so`).

---

## Architecture (Step 2)

### Overview

`be.so` is a pkg(8) plugin that intercepts pre-transaction hooks, creates a ZFS
boot environment via libbe, logs the result to syslog, and prunes excess
auto-created BEs. It is single-threaded and stateless between invocations: all
state is either in the config struct (loaded once at `pkg_plugin_init` time) or
in local variables on the call stack.

---

### File Layout and Responsibilities

```
pkg-be-plugin/
├── Makefile              build rules
├── pkg-be-plugin.c       plugin entry points + hook dispatch
├── pkg-be-plugin.h       shared types, the config struct, prototypes
├── config.c              config parsing (pkg_plugin_conf_add / parse / read)
├── config.h
├── prune.c               BE enumeration and pruning logic
├── prune.h
├── pkg-be-plugin.8       manpage
├── pkg.conf.sample       UCL snippet
└── tests/
    ├── Makefile
    └── test_config.c     ATF unit tests for config parsing
```

The split between files is functional:

- `pkg-be-plugin.c` knows about pkg; it does not call libbe directly.
- `prune.c` knows about libbe and nvpair; it does not call pkg APIs.
- `config.c` knows about both pkg's object API and the `be_config` struct.

This keeps each translation unit's `#include` set minimal and makes the ATF
tests for config and pruning compilable without a full pkg linkage.

---

### The Config Struct

Defined in `pkg-be-plugin.h`, allocated as a module-level static:

```c
struct be_config {
    bool     enabled;           /* BE_PLUGIN_ENABLED */
    int64_t  keep;              /* BE_PLUGIN_KEEP: max auto BEs to retain */
    time_t   min_age;           /* BE_PLUGIN_MIN_AGE: parsed from "7d" -> seconds */
    char     prefix[64];        /* BE_PLUGIN_NAME_PREFIX */
    bool     strict;            /* BE_PLUGIN_STRICT: abort on BE creation failure */
    bool     skip_install;      /* derived from BE_PLUGIN_SKIP_TRANSACTIONS */
    bool     skip_upgrade;
    bool     skip_deinstall;
};
```

A single `static struct be_config g_config;` lives in `pkg-be-plugin.c`. It is
written once during `pkg_plugin_init()` and read-only thereafter.

---

### Plugin Lifecycle

**`pkg_plugin_init(struct pkg_plugin *p)`**

```
1. pkg_plugin_set()              -- name, desc, version
2. config_register_keys(p)       -- pkg_plugin_conf_add() for each key with defaults
3. pkg_plugin_parse(p)           -- reads /usr/local/etc/pkg/be.conf (UCL)
4. config_load(p, &g_config)     -- walks pkg_plugin_conf() object, fills struct
5. if !g_config.enabled: return EPKG_OK (no hooks registered -- no-op plugin)
6. pkg_plugin_hook_register() x3
7. return EPKG_OK
```

If `config_load()` encounters a parse error (e.g., bad `min_age` string), it
logs via `pkg_plugin_error()` and returns `EPKG_FATAL`. A broken config is user
error; we do not silently fall back to defaults for something the user explicitly
set wrong.

**`pkg_plugin_shutdown(struct pkg_plugin *p)`**

Nothing to free. `g_config` is a static struct with no heap members (the prefix
is an in-struct array). Returns `EPKG_OK`.

---

### Hook Handler Design

All three hooks (`PRE_INSTALL`, `PRE_UPGRADE`, `PRE_DEINSTALL`) share a single
implementation function:

```c
static int  be_hook(void *data, struct pkgdb *db);
```

All three hook registrations point to this same function. The hook determines
which transaction type it's handling by calling
`pkg_jobs_type((struct pkg_jobs *)data)` and checking `g_config.skip_*` flags.

A small helper provides the human-readable name for log messages:

```c
static const char *
be_hook_name(pkg_jobs_t type)
{
    switch (type) {
    case PKG_JOBS_INSTALL:   return ("pre-install");
    case PKG_JOBS_UPGRADE:   return ("pre-upgrade");
    case PKG_JOBS_DEINSTALL: return ("pre-deinstall");
    default:                 return ("unknown");
    }
}
```

**Hook flow:**

```
be_hook(data, db):
    jobs = (struct pkg_jobs *)data
    type = pkg_jobs_type(jobs)

    switch type:
        INSTALL:   if g_config.skip_install:   return EPKG_OK
        UPGRADE:   if g_config.skip_upgrade:   return EPKG_OK
        DEINSTALL: if g_config.skip_deinstall: return EPKG_OK
        default:
            pkg_emit_notice("be-plugin: unhandled jobs type %d, skipping", type)
            return EPKG_OK

    generate_be_name(g_config.prefix, be_name, sizeof(be_name))

    hdl = libbe_init(NULL)
    if hdl == NULL:
        syslog(LOG_WARNING, "pkg-be-plugin: libbe_init failed")
        pkg_plugin_error(g_plugin, "libbe_init failed: not a ZFS BE system")
        error = 1; goto done

    libbe_print_on_error(hdl, false)

    if be_validate_name(hdl, be_name) != 0:
        syslog(LOG_WARNING, "pkg-be-plugin: invalid BE name: %s", be_name)
        pkg_plugin_error(g_plugin, "invalid BE name: %s", be_name)
        error = 1; goto done

    rc = be_create(hdl, be_name)
    if rc != BE_ERR_SUCCESS:
        syslog(LOG_WARNING, "pkg-be-plugin: be_create(%s) failed: %s",
            be_name, libbe_error_description(hdl))
        pkg_plugin_error(g_plugin, "%s: be_create failed: %s",
            be_hook_name(type), libbe_error_description(hdl))
        error = 1; goto done

    /* success */
    syslog(LOG_INFO, "pkg-be-plugin: created boot environment %s", be_name)
    pkg_emit_notice("be-plugin: created boot environment: %s", be_name)
    prune_old_bes(g_config.prefix, g_config.keep, g_config.min_age)

done:
    if hdl != NULL:
        libbe_close(hdl)
    if error && g_config.strict:
        syslog(LOG_ERR, "pkg-be-plugin: aborting transaction (%s)",
            be_hook_name(type))
        return EPKG_FATAL
    return EPKG_OK
```

The `g_plugin` pointer is stored at init time in a
`static struct pkg_plugin *g_plugin;` so the hook callback can call
`pkg_plugin_error()` and `pkg_plugin_info()` without threading it through
`data`.

---

### BE Name Generation

```c
static void
generate_be_name(const char *prefix, char *buf, size_t bufsz)
{
    time_t    now;
    struct tm tm;

    time(&now);
    localtime_r(&now, &tm);
    snprintf(buf, bufsz, "%s-%04d%02d%02d-%02d%02d%02d",
        prefix,
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);
}
```

`BE_MAXPATHLEN` is 512 bytes; BE names are much shorter. We use a fixed 128-byte
stack buffer. The name passes through `be_validate_name()` before `be_create()`.

---

### Config Parsing: `min_age` Duration

`BE_PLUGIN_MIN_AGE` is stored as a UCL string (`"7d"`, `"24h"`, `"30m"`,
`"0"`). The config parser converts it to `time_t` seconds during
`config_load()`. Implemented as a small handwritten function in `config.c`:

```
parse_duration(const char *s, time_t *out):
    n = strtol(s, &end, 10)
    if end == s: error (no digits)
    switch *end:
        'd': *out = n * 86400; break
        'h': *out = n * 3600;  break
        'm': *out = n * 60;    break
        '\0': *out = n;        break   /* bare seconds */
        default: error (unknown suffix)
```

`BE_PLUGIN_SKIP_TRANSACTIONS` is a comma-separated string. Parsed in `config.c`
with `strsep()` into the three bool flags. Unknown tokens produce a warning but
are not fatal.

---

### Pruning Algorithm (`prune.c`)

```
prune_old_bes(prefix, keep, min_age):
    hdl = libbe_init(NULL)
    if hdl == NULL: return   /* best-effort; log via syslog, not pkg API */

    be_prop_list_alloc(&list)
    be_get_bootenv_props(hdl, list)

    /* Collect BEs matching our prefix into a heap-allocated dynamic array.
     * Initial capacity BE_CAND_INIT, doubles on each realloc, hard cap
     * BE_CAND_MAX (4096) beyond which we log a warning and stop. */
    cap = BE_CAND_INIT   /* 8 */
    candidates = malloc(cap * sizeof(*candidates))
    count = 0
    for each nvpair in list:
        name = nvpair_name(pair)
        if strncmp(name, prefix, strlen(prefix)) == 0:
            if count == cap:
                if cap >= BE_CAND_MAX:
                    syslog(LOG_WARNING,
                        "pkg-be-plugin: >%d matching BEs, truncating prune scan",
                        BE_CAND_MAX)
                    break
                cap *= 2; candidates = realloc(candidates, cap * sizeof(*candidates))
            nvpair_value_nvlist(pair, &props)
            nvlist_lookup_uint64(props, "creation", &t)
            candidates[count].creation = (time_t)t
            strlcpy(candidates[count].name, name, sizeof(candidates[count].name))
            count++

    be_prop_list_free(list)

    if count <= keep:
        free(candidates)
        libbe_close(hdl)
        return

    /* Sort ascending by creation time (oldest first) */
    qsort(candidates, count, sizeof(*candidates), compare_creation)

    now = time(NULL)
    to_delete = count - keep
    deleted = 0
    deferred = 0   /* BEs skipped due to min_age */
    for i = 0 to count-1 while deleted < to_delete:
        age = now - candidates[i].creation
        if age < min_age:
            deferred++
            continue   /* respect min_age: skip even if over count */
        rc = be_destroy(hdl, candidates[i].name, BE_DESTROY_ORIGIN)
        if rc != BE_ERR_SUCCESS:
            syslog(LOG_WARNING, "pkg-be-plugin: prune: could not destroy %s",
                candidates[i].name)
        else:
            deleted++

    /* If min_age deferred pruning while we're significantly over keep, tell
     * the user once so they know the pruner is being conservative. */
    if deferred > 0 && count - deleted > keep:
        pkg_emit_notice(
            "be-plugin: %d auto-created BEs present, %d under min_age, "
            "pruning deferred (lower BE_PLUGIN_MIN_AGE to prune sooner)",
            count, deferred)

    free(candidates)
    libbe_close(hdl)
```

**Key decisions:**

- Dynamic array: initial capacity `BE_CAND_INIT = 8`, doubles on realloc, hard
  cap `BE_CAND_MAX = 4096`. Above the cap a `LOG_WARNING` syslog is emitted and
  collection stops. Silent truncation at a fixed size was rejected because the
  failure mode — pruner silently stops working, pool fills with BEs — is worse
  than the small extra code.
- Sort ascending by creation time (oldest first) before checking `min_age`, so
  the oldest BEs are deleted first when multiple are eligible.
- `min_age` is always hard-enforced. `keep` is a target, not a ceiling.
  The default of 7 days means recent BEs are never pruned even if count exceeds
  `keep`. Users who want stricter behavior lower `min_age` to `0` or `1h`.
- When `min_age` defers pruning and count is still above `keep`, emit one
  `pkg_emit_notice()` per pkg run (not per BE checked) explaining the situation.
- Pruning failures are logged to syslog but never affect the hook return value.
  Pruning is best-effort.
- We do not need to explicitly skip the active or nextboot BE: `be_destroy()`
  returns `BE_ERR_DESTROYACT` for the active one, which we treat as a non-fatal
  prune failure.

---

### Error Handling Strategy

#### Error reporting taxonomy

Each failure has a specific combination of syslog priority and pkg API call.
The rules are:

| Situation | syslog | pkg API |
|-----------|--------|---------|
| BE created successfully | `LOG_INFO` | `pkg_emit_notice()` |
| BE creation failed, non-strict | `LOG_WARNING` | `pkg_plugin_error()` |
| BE creation failed, strict (aborting) | `LOG_ERR` | `pkg_plugin_error()` |
| Prune `be_destroy()` failed | `LOG_WARNING` | *(none — prune is silent to the user)* |
| Prune deferred by `min_age` | *(none)* | `pkg_emit_notice()` once per run |
| `libbe_init()` failed in prune | `LOG_WARNING` | *(none)* |
| `config_load()` bad value | *(none — at init time)* | `pkg_plugin_error()` → EPKG_FATAL |

Rationale:
- `pkg_emit_notice()` is for the interactive user watching `pkg install` output.
  A successfully created BE is always shown here — it is the primary signal the
  plugin is working.
- `pkg_plugin_error()` is for failures the user needs to act on. In non-strict
  mode these are advisory; in strict mode they precede an abort.
- syslog `LOG_INFO` provides an audit trail of BE names for rollback discovery
  (`grep pkg-be-plugin /var/log/messages`). It is always written on success.
- syslog `LOG_WARNING` captures non-fatal failures for later diagnosis without
  interrupting the interactive session.
- syslog `LOG_ERR` is reserved for strict-mode aborts — something went wrong and
  pkg was stopped because of it.
- Prune operations are intentionally silent to the interactive user (syslog
  only) except for the `min_age` deferral notice, which needs user awareness
  because it means the pool may have more BEs than expected.

#### Failure disposition table

| Failure | strict=false | strict=true |
|---------|--------------|-------------|
| `libbe_init()` fails | `LOG_WARNING` + `pkg_plugin_error()` + continue | same + `LOG_ERR` + abort |
| `be_validate_name()` fails | `LOG_WARNING` + `pkg_plugin_error()` + continue | same + `LOG_ERR` + abort |
| `be_create()` fails | `LOG_WARNING` + `pkg_plugin_error()` + continue | same + `LOG_ERR` + abort |
| prune `be_destroy()` fails | `LOG_WARNING` + continue prune | `LOG_WARNING` + continue prune |
| `be_get_bootenv_props()` fails | `LOG_WARNING` + skip prune | `LOG_WARNING` + skip prune |
| `config_load()` bad value | `pkg_plugin_error()` + plugin fails to load | same |

Pruning never aborts the transaction regardless of `strict`, because pruning is
maintenance work that happens *after* the BE is already safely created.

**The goto-on-error pattern used in the hook handler:**

```c
static int
be_hook(void *data, struct pkgdb *db)
{
    libbe_handle_t  *hdl = NULL;
    int              error = 0;
    char             be_name[128];

    /* ... generate name, check skip flags ... */

    if ((hdl = libbe_init(NULL)) == NULL) {
        pkg_plugin_error(g_plugin, "libbe_init failed: not a ZFS BE system");
        error = 1;
        goto done;
    }
    libbe_print_on_error(hdl, false);

    /* ... be_validate_name, be_create ... */

done:
    if (hdl != NULL)
        libbe_close(hdl);
    if (error && !g_config.strict)
        return (EPKG_OK);
    return (error ? EPKG_FATAL : EPKG_OK);
}
```

The single `done:` label with a guarded `libbe_close()` ensures the handle is
closed on every code path without duplicating the close call.

---

### Makefile Approach

Use `bsd.lib.mk` with `SHLIB_NAME=be.so` and `NO_SHLIB_LINKS=yes`. This is the
canonical FreeBSD build system approach, required for eventual ports tree
inclusion (`sysutils/pkg-be-plugin`). The base system maintainers update
`bsd.lib.mk` when toolchain details change; the project inherits those fixes for
free across point releases.

`NO_SHLIB_LINKS=yes` suppresses creation of the `be.so -> be.so.0` symlink that
`bsd.lib.mk` would otherwise generate. The install target installs directly to
`/usr/local/lib/pkg/` rather than the default `SHLIBDIR`.

```makefile
.include <bsd.own.mk>

SHLIB_NAME=     be.so
NO_SHLIB_LINKS= yes
SRCS=           pkg-be-plugin.c config.c prune.c

CFLAGS+=        -I${LOCALBASE}/include
LDFLAGS+=       -L${LOCALBASE}/lib
LDADD+=         -lbe -lpkg

SHLIBDIR=       ${LOCALBASE}/lib/pkg

fmt:
	indent -nbad -bap -nbc -br -ce -ci4 -cli0 -d0 -di0 -i8 -ip -l79 \
	    -nlp -npcs -psl -sc -sob ${SRCS}

.include <bsd.lib.mk>
```

`LOCALBASE` defaults to `/usr/local` and is the ports-conventional variable for
the installed-software prefix. Using it instead of hardcoding `/usr/local` makes
the Makefile correct for non-default prefix builds.

---

### ATF Tests

`tests/test_config.c` tests `parse_duration()` and the `skip_transactions`
parser in isolation — pure string functions with no pkg or libbe dependency,
buildable and runnable without ZFS.

Test cases for `parse_duration`:
- `"7d"` → 604800
- `"24h"` → 86400
- `"30m"` → 1800
- `"300"` → 300 (bare seconds)
- `"0"` → 0
- `"bad"` → error return
- `"7x"` → error return (unknown suffix)
- `""` → error return

Test cases for `skip_transactions` parser:
- `"deinstall"` → skip_deinstall=true, others false
- `"install,upgrade"` → skip_install=true, skip_upgrade=true
- `""` → all false
- `"unknown"` → all false (warn, no abort)

Test cases for pruning sort (`compare_creation`):
- Hand-built `candidates` array with out-of-order timestamps asserts correct
  ascending sort order after `qsort()`.

---

## Design Decisions (resolved)

**1. Single hook function with dispatch.**
One `be_hook()` for all three transaction types, dispatching on
`pkg_jobs_type()`. A `be_hook_name()` helper returns the human-readable string
for log messages. The `default:` case in the dispatch switch emits
`pkg_emit_notice()` and returns `EPKG_OK` — exhaustive, no silent fall-through.

**2. Heap-allocated dynamic array for pruning candidates.**
Initial capacity `BE_CAND_INIT = 8`, doubles on realloc, hard cap
`BE_CAND_MAX = 4096`. Above the cap: `LOG_WARNING` syslog, collection stops.
Silent fixed-size truncation was rejected because the failure mode (pruner
silently breaks, pool fills with BEs) is worse than the extra code.

**3. `min_age` hard-enforced; `keep` is a target, not a ceiling.**
`min_age` always takes priority. Count may exceed `keep` if all recent BEs are
under `min_age`. When this happens, one `pkg_emit_notice()` per pkg run informs
the user. Manpage will document: "`keep` is a target; `min_age` protects recent
BEs from pruning regardless of count."

**4. `bsd.lib.mk` with `SHLIB_NAME=be.so` and `NO_SHLIB_LINKS=yes`.**
Required for ports tree inclusion. `LOCALBASE` used instead of hardcoded
`/usr/local`. Base system updates to `bsd.lib.mk` (toolchain, linker flags)
apply automatically.

**5. Error reporting taxonomy.**
Resolved as described in the Error Handling Strategy section above. Summary:
`pkg_emit_notice()` on success; `pkg_plugin_error()` on actionable failures;
syslog `LOG_INFO` for audit trail; `LOG_WARNING` for non-fatal failures;
`LOG_ERR` for strict-mode aborts. Prune operations are syslog-only except for
the `min_age` deferral notice.

---

## Implementation Order

Per the agreed process, each step compiles cleanly before the next begins:

1. Skeleton plugin — loads, registers hooks, does nothing
2. Config parsing — `config.c`, `config.h`, ATF tests
3. BE creation on pre-install hook
4. Hook expansion to upgrade and deinstall
5. Pruning logic — `prune.c`, `prune.h`
6. Strict mode handling
7. Manpage and README
Proposed: explicit rules with `bsd.own.mk` (simpler). Alternative: `bsd.lib.mk`
with `SHLIB_NAME=be.so` and `NO_SHLIB_LINKS=yes` (more idiomatic for ports).
