"""Pin Cortex-M4 dispatch/rebinding budgets and constexpr table placement."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--cxx', default='arm-none-eabi-g++')
parser.add_argument('--objdump', default='arm-none-eabi-objdump')
parser.add_argument('--build-dir', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
args.build_dir.mkdir(parents=True, exist_ok=True)

for optimization in ('-O2', '-Os'):
    obj = args.build_dir / ('dispatch' + optimization + '.o')
    subprocess.run([args.cxx, '-std=c++17', '-mcpu=cortex-m4', '-mthumb',
                    optimization, '-DNDEBUG', '-fno-exceptions', '-fno-rtti',
                    '-ffreestanding', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                    '-I' + str(root), '-c', str(root / 'tests/dispatch_probe.cpp'),
                    '-o', str(obj)], check=True)
    dump = subprocess.check_output([args.objdump, '-d', str(obj)], text=True)
    obj.with_suffix('.disasm').write_text(dump)
    for symbol in ('dispatch_ref', 'dispatch_own', 'dispatch_sbo', 'borrow_ref', 'bind_ref', 'assign_ref'):
        match = re.search(r'^[0-9a-f]+ <' + symbol + r'>:\n(.*?)(?=\n\n|\Z)', dump, re.M | re.S)
        if match is None:
            raise SystemExit('Missing disassembly for ' + symbol)
        instructions = re.findall(r'^\s*[0-9a-f]+:\s+(?:[0-9a-f]{4}\s+)+([a-z][a-z0-9.]*)\s', match[1], re.M)
        # Alignment padding after a return is not part of the executed path.
        if 'bx' not in instructions:
            raise SystemExit('Missing tail return in ' + symbol)
        instructions = instructions[:instructions.index('bx') + 1]
        if symbol.startswith('dispatch_'):
            if instructions != ['ldrd', 'bx']:
                raise SystemExit(f'{symbol}: expected ldrd + bx, got {instructions}')
        elif symbol == 'assign_ref':
            # GCC 13 uses five instructions at -Os; GCC 14 uses the same
            # six-instruction sequence as -O2, including for the 1.0 baseline.
            if len(instructions) > 6 or any(op in ('bl', 'blx') for op in instructions):
                raise SystemExit(f'{symbol}: pointer assignment budget exceeded: {instructions}')
        elif len(instructions) > 4 or any(op.startswith('b') for op in instructions[:-1]):
            raise SystemExit(f'{symbol}: reference rebinding budget exceeded: {instructions}')
        print(f'{optimization} {symbol}: ' + ', '.join(instructions))

    for standard in ('17', '20'):
        rom_obj = args.build_dir / f'rom-cxx{standard}{optimization}.o'
        subprocess.run([args.cxx, '-std=c++' + standard, '-mcpu=cortex-m4',
                        '-mthumb', optimization, '-DNDEBUG', '-fno-exceptions',
                        '-fno-rtti', '-ffreestanding', '-Wall', '-Wextra',
                        '-Wpedantic', '-Werror', '-I' + str(root), '-c',
                        str(root / 'tests/cortexm_probe.cpp'), '-o', str(rom_obj)],
                       check=True)
        symbols = subprocess.check_output([args.objdump, '-t', str(rom_obj)], text=True)
        sections = subprocess.check_output([args.objdump, '-h', str(rom_obj)], text=True)
        rom_obj.with_suffix('.layout').write_text(sections + '\n' + symbols)
        table = re.search(r'\s(\.rodata\S*)\s+([0-9a-f]+)\s+cortex_rom$', symbols, re.M)
        if table is None or int(table[2], 16) != 5 * 8:
            raise SystemExit('Expected five 8-byte cortex_rom entries in .rodata')
        for name, size in re.findall(r'^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s', sections, re.M):
            if name.startswith(('.data', '.bss', '.init_array', '.ctors')) and int(size, 16):
                raise SystemExit(f'{rom_obj.name}: unexpected writable/startup section {name}')
        print(f'{optimization} C++{standard} cortex_rom: 40 bytes in {table[1]}, no RAM/startup sections')
