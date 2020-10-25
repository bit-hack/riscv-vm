import subprocess
import os
import sys


# defaults
path_emu = os.path.join('..', 'build', 'Debug', 'riscv_vm.exe')
path_elf = os.path.join('compliance', 'elf')
path_ref = os.path.join('compliance', 'goldens')
path_out = os.path.join('compliance', 'out')


def run_generic(emu, elf):
    # run the tests
    try:
        args = [emu, elf]
        subprocess.check_call(args, timeout=10)
    except subprocess.TimeoutExpired as e:
        print('!-- TimeoutExpired --!', file=sys.stderr)
        return False
    except subprocess.CalledProcessError as e:
        print('!-- CalledProcessError --!', file=sys.stderr)
        return False
    # success
    return True

def run_test(emu, elf, ref, out):
    # run the tests
    try:
        args = [emu, elf, '--compliance']
        output = subprocess.check_output(args, timeout=5)
    except subprocess.TimeoutExpired as e:
        print('!-- TimeoutExpired --!', file=sys.stderr)
        return False
    except subprocess.CalledProcessError as e:
        print('!-- CalledProcessError --!', file=sys.stderr)
        return False
    # get the output
    lines = output.splitlines(keepends=False)
    with open(out, 'wb') as fd:
        fd.write(output)
    with open(ref, 'r') as fd:
        reflines = fd.readlines() or []
    # compare the number of lines
    num_lines = min(len(lines), len(reflines))
    for i in range(0, num_lines):
        l = lines[i].decode()
        r = reflines[i].strip()
        if l != r:
            return False
    # check numlines matches
    if (len(lines) != len(reflines)):
        print('!-- OutputQuantityMismatch --!', file=sys.stderr)
        return False
    # success
    return True


def main():
    global path_emu

    if len(sys.argv) >= 2:
        path_emu = sys.argv[1]

    for file in os.listdir(path_elf):
        name = os.path.splitext(file)
        file_elf = os.path.join(path_elf, file)
        file_ref = os.path.join(path_ref, name[0] + '.reference_output')
        file_out = os.path.join(path_out, name[0] + '.reference_output')
        res = run_test(path_emu, file_elf, file_ref, file_out)
        print('{0:>26} [{1}]'.format(file, 'ok' if res else 'fail'))

    elf1 = os.path.join('towers', 'towers.elf')
    res1 = run_generic(path_emu, elf1)
    print('{0:>26} [{1}]'.format(elf1, 'ok' if res1 else 'fail'))

    elf2 = os.path.join('rsort', 'rsort.elf')
    res2 = run_generic(path_emu, elf2)
    print('{0:>26} [{1}]'.format(elf2, 'ok' if res2 else 'fail'))

    elf3 = os.path.join('pi', 'pi.elf')
    res3 = run_generic(path_emu, elf3)
    print('{0:>26} [{1}]'.format(elf3, 'ok' if res3 else 'fail'))

    elf4 = os.path.join('puzzle', 'puzzle.elf')
    res4 = run_generic(path_emu, elf4)
    print('{0:>26} [{1}]'.format(elf4, 'ok' if res4 else 'fail'))

main()
