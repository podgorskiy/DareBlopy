import re


def parse_symbols(logfile_path):
    regexp_iter = r'(\S*) (\S) (\S*)'

    re_iter = re.compile(regexp_iter)

    symbols = []

    with open(logfile_path, 'r') as f:
        for line_terminated in f:
            line = line_terminated.rstrip('\n')
            r = re_iter.findall(line)
            if len(r) == 0:
                continue
            if r[0][1] != 'U':
                symbols.append(str(r[0][2]))

    return symbols


def main():
    vanila_symbols = set(parse_symbols('libjpeg-vanila-symbols.txt'))
    turbo_symbols = set(parse_symbols('libjpeg-turbo-symbols.txt'))

    #assert 'jpeg_destroy_decompress' in turbo_symbols
    #assert 'jpeg_destroy_decompress' in vanila_symbols

    collision_symbols = []

    print('#ifdef TURBO')

    for s in turbo_symbols:
        if s in vanila_symbols:
            collision_symbols.append(s)

    for s in collision_symbols:
        print("#define %s %s_turbo" % (s, s))

    print('#include "jconfig-turbo.h"\n#elif defined(VANILA)')

    for s in collision_symbols:
        print("#define %s %s_vanila" % (s, s))

    print('#include "jconfig-vanila.h"\n#else\n#error Error! JPEG library not defined! Use TURBO or VANILA\n#endif')

if __name__ == '__main__':
    main()
