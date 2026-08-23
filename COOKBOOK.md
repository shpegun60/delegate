# tiny_delegate cookbook

Ten real-world recipes covering every part of the library. Each one was
compiled and executed with asserts before landing here — the commented
results are the verified ones. Pitfalls at the end were hit for real
while writing the recipes.

## 1. ROM dispatch table — commands by opcode

Compile-time bindings make `delegate_ref` fully constexpr-constructible,
so a dispatch table lives in flash (verified in `.rodata`, 8 bytes per
entry on ARM32, zero RAM, zero startup code):

```cpp
static int cmdPing(int)   { return 1; }
static int cmdReset(int)  { return 2; }
static int cmdEcho(int x) { return x; }

static constexpr tiny::delegate_ref<int(int)> k_commands[] = {
    tiny::delegate_ref<int(int)>::bind<&cmdPing>(),
    tiny::delegate_ref<int(int)>::bind<&cmdReset>(),
    tiny::delegate_ref<int(int)>::bind<&cmdEcho>(),
};

int dispatch(unsigned op, int arg) { return k_commands[op](arg); }
```

## 2. Rebindable event slot — empty is legal

A subscription point that may have no subscriber. `call_if` makes the
empty state explicit instead of trapping:

```cpp
tiny::delegate_ref<void(int)> on_sample;      // nobody subscribed yet

void publish(int v) { (void)on_sample.call_if(v); }  // silently skipped

on_sample = tiny::borrow(subscriber);          // subscribe
on_sample.reset();                             // unsubscribe
```

## 3. Driver completion callback — owning, temporaries welcome

The driver owns its callback, so the application may hand it a temporary
capturing lambda and walk away:

```cpp
struct Uart {
    tiny::delegate_sbo<void(bool), 16> on_done;      // driver owns it
    void irqTxComplete() { on_done.call_if(true); }  // no-op if unset
};

uart.on_done = [&stats](bool ok) { if (ok) ++stats.tx_done; };  // temporary!
```

## 4. Observer — method bound to a runtime instance

```cpp
struct Display { void show(int v); };
Display lcd;

auto view = tiny::delegate<void(int)>::bind<&Display::show>(lcd);
view(123);          // lcd.show(123); non-owning, lcd must outlive view
```

## 5. State machine — states as a compile-time delegate table

States of a long-lived object, baked into flash; switching states is an
index change, not a rebind:

```cpp
static Fsm g_fsm;
static constexpr tiny::delegate_ref<void()> k_states[] = {
    tiny::delegate_ref<void()>::bind<&Fsm::onIdle, g_fsm>(),
    tiny::delegate_ref<void()>::bind<&Fsm::onRun,  g_fsm>(),
};

void tick() { k_states[current_state](); }
```

## 6. Policy with a default — `call_or`

An override point that falls back to the standard implementation while
nobody has installed a custom one:

```cpp
tiny::delegate<int(int)> crc_override;                  // empty = standard

int computeCrc(int x) { return crc_override.call_or(&stdCrc, x); }

crc_override = [](int x) { return customCrc(x); };      // install override
crc_override = nullptr;                                 // back to standard
```

## 7. Move-only callable — owning a resource

A callable that owns a non-copyable resource moves straight into the
delegate; the delegate stays move-only end to end:

```cpp
tiny::delegate_sbo<int()> closer{[h = FileHandle{fd}] { return h.close(); }};
auto transferred = std::move(closer);   // resource moved, `closer` empty
```

## 8. Hybrid member — owns small closures, borrows big services

One field serves both modes; introspection tells them apart:

```cpp
struct Pipeline { tiny::delegate<int(int), 16> stage; };

p.stage = [](int x) { return x + 1; };                 // small -> owns inline
p.stage = decltype(p.stage)::bind<&BigService::process>(g_service); // borrows
p.stage.owning();  p.stage.non_owning();               // honest either way
```

## 9. Owning stage chain

A fixed pipeline of independently-configured steps, no heap anywhere:

```cpp
tiny::delegate_sbo<int(int), 16> stages[3];
stages[0] = [](int x) { return x + 1; };
stages[1] = [](int x) { return x * 2; };
stages[2] = [](int x) { return x - 3; };

int v = input;
for (auto& s : stages) v = s(v);
```

## 10. Delegate as a tickcore timer handler

With [tickcore](https://github.com/shpegun60/tickcore) the composition is
free — a delegate is just a callable to the scheduler:

```cpp
timers::DelegateScheduler<Clk, 4u> sched;

tiny::delegate<bool()> task{[&n] { return ++n < 2; }};   // repeat-while
(void)sched.every(10u, [&task] { return task(); });
```

(or hand the temporary lambda to the scheduler directly — its slots own
their handlers the same way `delegate_sbo` does.)

## Pitfalls hit while writing these

- **`InlineAlign` is per-platform.** `delegate<Sig, 16, 4>` is fine on
  ARM32 and refuses to compile on x64, where a function pointer needs
  8-byte alignment — the layout guard catches it. Portable code either
  omits the alignment (the default adapts) or uses `alignof(void*)`.
- **The free `tiny::bind<...>(obj)` returns a *default-sized*
  `tiny::delegate`.** Assigning it to a delegate with different
  `InlineBytes` does not convert — it tries to nest one delegate inside
  another and fails the fit check. When the destination type is not the
  default, call *its own* `bind`:
  `decltype(field)::bind<&T::method>(obj)`.
- **Compile-time instance binding sees the live object.** The instance is
  referenced, not copied: mutate `g_fsm` after building the table and the
  table's calls observe the change. That is exactly what you want for
  state tables — and worth knowing before you expect a snapshot.
