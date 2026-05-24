# QSettings Mapper

A small, header-only **declarative mapping layer** between C++ classes and
`QSettings`. Describe how your class maps onto a settings tree once, with a
template-based recipe, and get **type-safe** `save()` / `load()` for free.

The whole library is **one header**. No build step, no code generation, no
macros.

```cpp
using MySettings = Group<MyClass, "MyClass",
    SettingsGS<MyClass, &MyClass::name,  &MyClass::setName,  "name">,
    SettingsGS<MyClass, &MyClass::value, &MyClass::setValue, "value", 42>,
    Group<MyClass, "advanced",
        SettingsVar<MyClass, &MyClass::m_flag, "flag", false>
    >
>;

SettingsHelper::save<MySettings>(qsettings, obj);
SettingsHelper::load<MySettings>(qsettings, obj);
```

That's the whole API. Two functions, three building blocks.

---

## Why this exists

`QSettings` is wonderful for "store something somewhere", but the moment you
have more than a handful of properties to persist, the code becomes a wall of
copy-paste:

```cpp
settings.beginGroup("Window");
settings.setValue("title", win->title());
settings.setValue("width", win->width());
settings.setValue("height", win->height());
settings.setValue("opacity", win->opacity());
settings.endGroup();
// ... and the same again, mirrored, for load()
```

Every Qt project rebuilds this. The names get out of sync between save and
load. Adding a new field means touching two functions. Adding a default
fallback means another branch each time.

This library replaces all of that with a single declarative description.
Add a new field → add one line. Done.

---

## How it works

There are three primitives:

### 1. `SettingsGS` — getter/setter pair

For properties exposed through `Q_PROPERTY` or any get/set member functions:

```cpp
SettingsGS<MyClass, &MyClass::name, &MyClass::setName, "name">
```

The getter's return type is deduced; the setter must accept the same type.
A compile-time assertion makes sure those line up.

### 2. `SettingsVar` — direct member pointer

For internal fields you want to persist without exposing accessors:

```cpp
SettingsVar<MyClass, &MyClass::m_flag, "flag">
```

### 3. `Group` — keyed container

Bundles items under a `beginGroup()` / `endGroup()` pair. Groups nest:

```cpp
Group<MyClass, "advanced",
    SettingsVar<MyClass, &MyClass::m_flag, "flag">,
    Group<MyClass, "deeper",
        SettingsGS<MyClass, &MyClass::level, &MyClass::setLevel, "level">
    >
>
```

Each `Group` produces one level of nesting in the resulting INI/Registry
tree. The `class T` parameter is repeated at every level because the engine
needs to know what type of object it's working on (this is what enables
direct member-pointer access without any RTTI).

---

## Defaults

Both `SettingsGS` and `SettingsVar` take an optional fifth template
parameter — the default value used when a key is missing on load:

```cpp
// Plain value
SettingsGS<MyClass, &MyClass::port, &MyClass::setPort, "port", 8080>

// Callable default — invoked at load time
constexpr auto current_user = [] { return qgetenv("USER"); };
SettingsGS<MyClass, &MyClass::user, &MyClass::setUser, "user", current_user>
```

If the key is missing AND a default is declared, the default is applied and
the load proceeds silently. If the key is missing AND no default is declared,
the engine emits a `qWarning` and leaves the field untouched.

`QString` fields get an automatic conversion from any `toQString`-compatible
default (`const char*`, `std::string_view`, etc.).

---

## Full example

A complete worked example, end to end:

```cpp
#include "qsettings_helper.h"

class AppConfig {
public:
    QString  serverUrl() const               { return m_serverUrl; }
    void     setServerUrl(const QString& v)  { m_serverUrl = v; }
    int      port() const                    { return m_port; }
    void     setPort(int v)                  { m_port = v; }

    bool     m_useProxy = false;   // exposed directly via SettingsVar
    QString  m_proxyHost;

private:
    QString  m_serverUrl;
    int      m_port = 0;
};

using namespace SettingsHelperNs;

using AppConfigSettings = Group<AppConfig, "AppConfig",
    SettingsGS <AppConfig, &AppConfig::serverUrl, &AppConfig::setServerUrl, "server", "https://example.com">,
    SettingsGS <AppConfig, &AppConfig::port,      &AppConfig::setPort,      "port",   443>,
    Group<AppConfig, "proxy",
        SettingsVar<AppConfig, &AppConfig::m_useProxy,  "enabled",  false>,
        SettingsVar<AppConfig, &AppConfig::m_proxyHost, "host",     "">
    >
>;

void writeConfig(const AppConfig& cfg) {
    QSettings s("MyOrg", "MyApp");
    SettingsHelper::save<AppConfigSettings>(s, cfg);
}

void readConfig(AppConfig& cfg) {
    QSettings s("MyOrg", "MyApp");
    SettingsHelper::load<AppConfigSettings>(s, cfg);
}
```

The resulting `QSettings` tree looks like:

```ini
[AppConfig]
server=https://example.com
port=443

[AppConfig/proxy]
enabled=false
host=
```

Adding a new persisted field? Add one line to `AppConfigSettings`. The save
and load paths stay in sync by construction.

---

## What it gives you over hand-written `QSettings` code

- **Single source of truth.** Save/load can't drift because they're driven
  by the same description.
- **Compile-time validation.** Wrong getter/setter pair, wrong default type,
  wrong member pointer — all caught by `static_assert`.
- **Nesting for free.** `Group` recurses without extra code.
- **Defaults inline.** No separate "default config" function.
- **No virtual dispatch, no allocations, no macros.** Everything resolves at
  compile time; the generated code is essentially the same hand-written
  `setValue` / `value` calls you would have written, minus the bookkeeping.

---

## Bug fixes and refactor

This code was used internally in a Qt project; the public release is a
**cleaned-up rewrite**. Notable changes from the original:

- Fixed an incorrect fold in `Group`'s `static_assert` — the original
  accepted *any* item as long as one was valid; the corrected version
  rejects mixed items at compile time.
- Fixed typo `DefaulAsCallable` → `DefaultAsCallable`.
- Translated the few Italian comments to English.
- Migrated to the namespaces of the companion
  [TemplateHelper](https://github.com/96Ven96/cpp-template-toolkit) library
  (`FunctionTraitsNs::remove_cvref_t`, `ConversionTypesNs::Qt_::*`).
- Added Doxygen-style documentation.

---

## Dependencies

- **Qt 5.15+ or Qt 6.x** — `QSettings`, `QString`, `QVariant`.
- The companion utilities header
  [TemplateHelper](https://github.com/96Ven96/cpp-template-toolkit) — drop
  the headers into your include path; no build step required.
- **C++17** minimum; **C++20** preferred (cleaner template-lambda unpacking
  is selected automatically).

Drop `qsettings_helper.h` into your project and `#include` it. That's all.

---

## License

Released under the [MIT License](LICENSE) — free to use, modify, and
redistribute, including in commercial and closed-source projects.

---

## Contact

- GitHub — [@96Ven96](https://github.com/96Ven96)
- LinkedIn — [Leonardo Rossi](https://www.linkedin.com/in/leonardo-rossi-262641203)
