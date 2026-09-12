# Delegate audit — 2026-09-12

Audited the C++17/C++20 header, all three delegate types, configuration and
lifetime contracts, CMake tests, Qt test bench, comments, documentation and CI.
The starting revision was `3d970b5e92a950fbcd52ac3153b28d23c99092c2`.
The final header's SHA-256 in the audited working tree is
`5cdfd130e85709b302bf02e6e769d05136fd4509290cb90ef3ae83098e52f4eb`.
Line-ending conversion during checkout can change this byte-level hash.

All local validation below passed. This records a tested contract and regression
coverage; it is not a proof that arbitrary application callbacks are safe.

## Corrections and compile-time protection

- Assignment from a reference to the currently owned callable previously
  destroyed the source before copying it. A minimal original-header reproduction
  produced an ASan `heap-use-after-free`. Assignment now preserves an aliased
  source before reset. The regression runs for both owning types with heap
  fallback enabled and disabled.
- Reset now clears ownership metadata before destroying the target. A target
  destructor can reset its owner again without recursively destroying itself.
  Inline moves clear the source before its moved-from target is destroyed.
- `bind` rejects null function/member template arguments and temporary objects.
  Explicitly specifying a reference as the object template parameter no longer
  bypasses temporary-object rejection.
- Construction and assignment check invocation, return-reference compatibility,
  source construction, non-throwing destruction, capacity, alignment and move
  policy during overload resolution. Deleted diagnostic overloads name the
  failure and storage dimensions while keeping `std::is_constructible` and
  `std::is_assignable` truthful. `fits_inline` remains a size/alignment query.
- Reference results must already refer to a compatible type; adaptation cannot
  manufacture a temporary. This deliberately rejects implicit user-defined
  reference conversions. GCC 13 already rejected some direct dangling-result
  examples in the original header; those are additional portable constraints
  and trait/diagnostic improvements, not newly demonstrated original UB.
- Function-pointer conversions use the actual source cv/ref category. Valid
  converters no longer instantiate an unnecessary owning-storage path.
  `call_or` handles `void` result adaptation and only inspects/invokes a fallback
  when the delegate is empty. `call_if` explains unsupported reference or
  immovable results.
- Null adapted callables and null borrowed pointers follow the assert policy
  and leave an empty delegate if the hook returns. A class-specific non-throwing
  allocator returning null also leaves the delegate empty. Invocation through
  a borrowed pointer is checked in Debug.
- `delegate_ref` stores object and function pointers in separate union members.
  It no longer relies on conditionally supported conversions between these
  pointer categories. Its two-word layout and constexpr ROM bindings remain.
  The relevant language rules are [pointer conversions](https://eel.is/c++draft/expr.reinterpret.cast)
  and [INVOKE result adaptation](https://eel.is/c++draft/func.require).
- The public header has an include guard alongside `#pragma once`. Redundant
  comments were removed and implementation/test comments were corrected.
  README contracts now cover exceptions, reentrancy, borrowed lifetimes,
  concurrency and configuration consistency.

## Validation

| Toolchain / target | Configurations | Result |
|---|---|---|
| GCC 13.3.0, WSL Ubuntu 24.04 | C++17/20, Debug/Release; C++20 ASan + UBSan | 5 × 12 CTest entries passed |
| Clang 18.1.3, WSL Ubuntu 24.04 | C++17/20, Debug/Release; C++20 ASan + UBSan | 5 × 12 CTest entries passed |
| MSVC 19.50.35719.0 | C++17/20, Debug/Release | 4 × 11 CTest entries passed |
| Qt 6.10.1, MinGW 13.1 | Console suite; Debug/Release offscreen UI autorun | 145 checks passed; both UI runs exited successfully |
| ARM GCC 13.2.1 | Cortex-M0/M3/M4/M7/M33 × C++17/20 × assert enabled/disabled | 20 compile configurations passed |

GCC/Clang builds used `-Werror`; MSVC used `/WX`. ARM builds used
`-fno-exceptions -fno-rtti -ffreestanding`. Sanitizer configurations covered both
heap policies. Each GCC/Clang CTest run included 36 negative compilation cases:
a valid control must compile, then each invalid source must fail with its
expected diagnostic. MSVC ran the runtime suites and their static assertions;
the negative compiler-output harness currently targets GCC/Clang.

Checks remain active in Release while the library uses its actual `NDEBUG`
policy. POSIX Debug tests verify trap signals in subprocesses. Exception tests
check failed copies/allocations, leak accounting and reuse. Cross-TU tests
exercise ownership transfer, destruction and reference-manager identity.
Over-aligned allocation counters cover aligned `new`/`delete`.

The Qt bench previously assigned typed null function pointers as a reset test,
which triggers the documented Debug trap. It now uses `nullptr` for reset;
dedicated policy tests cover invalid typed-null input.

## Generated code and storage

Compared object disassembly from the original and final headers using the same
Cortex-M4 compiler, probe, `-DNDEBUG`, and optimization settings. The instruction
sequences below match. Counts exclude alignment padding and literal data.

| Probe | `-O2`, original → final | `-Os`, original → final |
|---|---|---|
| `int(int)` dispatch, each of the three delegate types | 2 → 2 (`ldrd; bx`) | 2 → 2 (`ldrd; bx`) |
| `delegate_ref` function-pointer assignment | 6 → 6 | 5 → 5 |
| `delegate_ref` borrow assignment | 3 → 3 | 4 → 4 |
| `delegate_ref` method binding | 3 → 3 | 4 → 4 |

ARM assertions pin `delegate_ref<void()>` to 8 bytes and both default owning
types to 32 bytes. The static binding probe has an 8-byte `.rodata` entry with
no dynamic initializer. [check_codegen.py](tests/check_codegen.py) enforces
dispatch and rebinding budgets using [dispatch_probe.cpp](tests/dispatch_probe.cpp).

These results describe the supplied probes, not cycle measurements or every
possible signature/compiler. Owning functor assignment may need a source-address
comparison and, on aliasing, a temporary copy/move to preserve its source.
That work occurs during assignment, outside the invocation path. Callable
bodies and their argument/result operations can still allocate or throw.

## Reproduction and contract limits

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 tests/check_codegen.py --build-dir build-arm-codegen
```

The code-generation command requires `arm-none-eabi-g++` and
`arm-none-eabi-objdump`. Local logs, baseline reproductions, compiler matrices
and disassembly are retained in the ignored `build-audit/` directory.
The updated workflow defines 16 host jobs, two sanitizer jobs and one ARM job;
its YAML and matrix were checked locally. Remote results must be taken from the
CI run for the exact published commit; the matrix above describes local
execution. No macOS or target hardware execution was performed locally.

Borrowed targets, reference captures and returned references require externally
valid lifetimes. Concurrent mutation needs caller synchronization. Rebinding
from target constructors/destructors and assignment from a subobject destroyed
by replacement are outside the contract; see [README.md](README.md).
Empty `operator()` remains invalid, with the default check removed by `NDEBUG`.
Use `call_if` or `call_or` when empty is an expected state.

Rebuild every consumer with the same header revision and configuration:
the reference invoker's internal type changed even though its size did not.
