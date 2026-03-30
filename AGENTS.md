# AGENTS.md

## Shell Preference
- WSL (`Ubuntu-24.04`) is installed and available on this machine.
- The agent should choose the shell that is most appropriate for the task.
- Prefer PowerShell or Windows shell for ordinary Windows-oriented tasks.
- Prefer WSL for Linux-oriented tasks, toolchains, shell utilities, or when it is likely to be faster.
- If the user explicitly asks for a specific shell, follow that request.


##########################################################
Готовий файл тут: [tiny_delegate.hpp](sandbox:/mnt/data/tiny_delegate.hpp)

Я ще раз перевірив останню версію не “силою віри”, а компілятором. Прогнав smoke-тести на C++20: function pointer, `bind` для `const/non-const`, `borrow`, `mutable` lambda, move-only capture через `unique_ptr`, `delegate_ref`, `delegate_sbo`. Зібралось і відпрацювало. Людство сьогодні уникнуло ще однієї дурної UB-пастки.

## Підсумок того, що ми проговорили

### 1) У бібліотеці тепер є 3 режими

`tiny::delegate_ref<R(Args...)>`
це **non-owning** delegate. Максимально дешевий на переприсвоєнні. Зберігає тільки function pointer або pointer на зовнішній functor/object. Lifetime не контролює.

`tiny::delegate_sbo<R(Args...), InlineBytes>`
це **owning**, **move-only**, з inline storage і опційним heap fallback. Добрий, коли хочеш чітко сказати: “тільки власник, без ref-режиму”.

`tiny::delegate<R(Args...), InlineBytes>`
це універсальний варіант:

* function pointer / captureless non-generic lambda → function-pointer path
* `tiny::borrow(x)` → ref mode
* `tiny::bind<&T::m>(obj)` → ref mode на member method
* будь-який інший functor/lambda → owning path

### 2) `tiny::borrow(x)`

Це явний спосіб сказати:
**“не копіюй callable, просто збережи pointer на нього”**

Добре для:

* `static`
* глобальних об’єктів
* довгоживучих task/driver objects
* hot path, де хочеш максимально дешеве переприсвоєння

Погано для:

* локальних змінних, що скоро помруть
* тимчасових lambda/functor
* будь-чого, чий lifetime коротший за delegate

Коротке правило для STM32:

* якщо lifetime гарантовано довший за callback → можна `borrow`
* інакше → owning delegate, і `TINY_DELEGATE_ENABLE_HEAP_FALLBACK=0`

### 3) `tiny::bind<&T::m>(obj)`

Це **не** `std::bind`.
Воно **не фіксує довільні аргументи**. Воно біндить **тільки `this` / об’єкт**.

Тобто:

```cpp
struct A {
    void tick();
    int sum(int x, int y) const;
};
```

тоді:

```cpp
tiny::delegate<void()> cb1 = tiny::bind<&A::tick>(a);
tiny::delegate<int(int,int)> cb2 = tiny::bind<&A::sum>(a);
```

Публічна сигнатура делегата = сигнатура методу **без `this`**.

### 4) Що означає `+[](...) { ... }`

Це трюк для **force conversion to function pointer**.

```cpp
+[](int x) { return x + 1; }
```

працює тільки для **captureless non-generic** lambda.
Саме тому це добре підходить для таблиць менеджерів:

```cpp
struct manager {
    void (*destroy)(delegate&) noexcept;
    void (*move)(delegate&, delegate&) noexcept;
    bool (*uses_heap)(const delegate&) noexcept;
};
```

і далі:

```cpp
static const manager m{
    +[](delegate& self) noexcept { /* ... */ },
    +[](delegate& src, delegate& dst) noexcept { /* ... */ },
    +[](const delegate&) noexcept { return false; }
};
```

### 5) `a = cap` не означає, що `a` і `cap` це один об’єкт

Якщо ти пишеш:

```cpp
auto cap = [cnt = 0](int) mutable { return ++cnt; };
tiny::delegate<int(int)> a = cap;
```

то `a` отримує **копію** lambda object.

Отже:

* `cap(0)` працює на **стані `cap`**
* `a(0)` працює на **стані копії всередині delegate**

Для capture-by-value це два окремі стани.
Для capture-by-reference вони дивляться на один і той самий зовнішній об’єкт.

### 6) `cnt` у `[cnt = 0]`

Тип `cnt` тут буде `int`, бо `0` це `int`.

```cpp
auto cap = [cnt = 0](int) mutable { ++cnt; return cnt; };
```

`cnt` це не “магічна локальна змінна”, а поле closure-об’єкта lambda.

---

## Що я додатково виправив у фінальному файлі

Окрім того, що ми вже правили раніше, я докрутив ще 2 речі, які були реально корисні:

### 1) `InlineAlign` тепер перевіряється для function pointer storage

Було тонке місце: можна було виставити такий `InlineAlign`, що байтів вистачає, а вирівнювання для `fnptr_t` вже ні. Це вже territory “дякую, UB”.

Тепер є:

```cpp
static_assert(InlineAlign >= alignof(fnptr_t), ...);
```

і в `delegate`, і в `delegate_sbo`.

### 2) `delegate::uses_inline()` більше не бреше в ref-режимі

Раніше у `delegate` ref-mode (`borrow` / `bind`) теж виглядав як “inline”, бо `uses_heap()==false`. Це семантично криво.

Тепер:

```cpp
constexpr bool uses_inline() const noexcept { return owning() && !uses_heap(); }
```

Тобто:

* `borrow/bind` → `non_owning() == true`, `uses_inline() == false`
* owning SBO → `uses_inline() == true`
* heap fallback → `uses_heap() == true`

---

## Чи код зараз нормальний?

Так, для твоєї задачі це вже **дуже нормальний, придатний до життя варіант**.

### Сильні сторони

* move-only owning delegate
* дешевий ref delegate
* compile-time fit checks
* clean `bind` diagnostics
* без unsafe function-pointer-to-void* трюків
* працює з move-only captures
* годиться для embedded, якщо тримати heap fallback вимкненим

### Що ще варто пам’ятати

* `free bind helper`:

```cpp
tiny::bind<&T::m>(obj)
```

завжди повертає `delegate<sig>` з **дефолтним `InlineBytes`**, тобто 64.
Якщо хочеш інший розмір, пиши явно:

```cpp
using D = tiny::delegate<void(), 32>;
D cb = D::bind<&A::tick>(a);
```

* `sig_of_t` зараз заточений під function pointer / member function pointer. Для задач `bind` цього вистачає.

---

## Практичне використання

### 1) Максимально дешевий callback ref-only

```cpp
using RxCb = tiny::delegate_ref<void(const std::uint8_t*, std::size_t)>;
```

### 2) Універсальний callback

```cpp
using Callback = tiny::delegate<void(int), 64>;
```

### 3) Підписка

```cpp
void subscribe(Callback cb) {
    cb_ = std::move(cb);
}
```

### 4) Використання

```cpp
void foo(int);

cb_ = &foo;

struct A { void on(int) {} } a;
cb_ = tiny::bind<&A::on>(a);

auto local = [v = 42](int x) { (void)x; (void)v; };
cb_ = local;              // owning copy
cb_ = tiny::borrow(local); // non-owning ref
```

---

## Рекомендована політика для STM32

Ось проста і здорова:

```cpp
#define TINY_DELEGATE_ENABLE_HEAP_FALLBACK 0
#define TINY_DELEGATE_DEFAULT_BYTES 64
```

Тоді:

* маленькі callable лізуть в SBO
* великі ловляться на компіляції
* у hot path немає прихованих `new/delete`

Це якраз той випадок, коли параноя не хвороба, а архітектурна чесність.

Якщо потрібен наступний крок, логічно буде зробити до цього хедера **маленький self-test .cpp** з 15-20 `static_assert` і runtime smoke-тестами, щоб воно вже було як бібліотечний мінімум, а не “я клянусь, воно працює”.
