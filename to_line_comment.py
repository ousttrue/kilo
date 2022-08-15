import sys
import pathlib
import re


def repl(m):
    text = m.group(1)
    if '\n' not in text:
        return '//' + text + '\n'

    lines = []
    for i, l in enumerate(text.splitlines()):
        if i == 0:
            lines.append('//' + l + '\n')
        else:
            l = re.sub(r'^\s*\*', '', l)
            if l:
                lines.append('//' + l + '\n')

    return ''.join(lines)


def convert(src: str) -> str:
    return re.sub(r'/\*([\s\S]*?)\*/', repl, src)


if __name__ == '__main__':
    src = pathlib.Path(sys.argv[1])
    dst = pathlib.Path(sys.argv[2])

    result = convert(src.read_text(encoding='utf-8'))
    dst.write_text(result, encoding='utf-8')
