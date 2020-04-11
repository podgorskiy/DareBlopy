# Copyright 2019-2020 Stanislav Pidhorskyi
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================

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
    # The files that are parsed here contain all symbols from libjpeg and libjpeg-turbo
    vanila_symbols = set(parse_symbols('libjpeg-vanila-symbols.txt'))
    turbo_symbols = set(parse_symbols('libjpeg-turbo-symbols.txt'))

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
