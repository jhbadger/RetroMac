#!/usr/bin/env python3
"""pstr_rewrite.py -- rewrite MPW/Retro68-style "\\p..." Pascal-string
literals into plain-C compound literals that stock clang understands.

Classic Mac sources write Pascal (length-prefixed) strings as
`"\\pHello"` -- a nonstandard escape MPW/Retro68's compiler recognizes
specially. Stock clang has no such extension, so `retromacc` runs
every user source through this rewriter first, turning:

    "\\pHello"

into:

    ((unsigned char[]){5,72,101,108,108,111,0})

i.e. a length byte, the raw Mac-Roman bytes, and a trailing NUL (so it
still prints sanely if ever misused as a C string). Byte values are
used instead of re-escaped characters so the rewrite never has to
worry about quoting a `"` or `\\` inside the literal.

Only string literals are touched; comments and character literals are
skipped so a stray `\\p` inside a comment can't be misinterpreted.
"""
import sys


def decode_escapes(body):
    """Decode a C string literal body (no surrounding quotes) into a
    list of byte values, per ordinary C escape rules."""
    out = []
    i = 0
    n = len(body)
    while i < n:
        c = body[i]
        if c == '\\' and i + 1 < n:
            nc = body[i + 1]
            if nc == 'n':
                out.append(10); i += 2
            elif nc == 't':
                out.append(9); i += 2
            elif nc == 'r':
                out.append(13); i += 2
            elif nc == '\\':
                out.append(92); i += 2
            elif nc == '"':
                out.append(34); i += 2
            elif nc == "'":
                out.append(39); i += 2
            elif nc == 'x':
                j = i + 2
                hexdigits = ''
                while j < n and body[j] in '0123456789abcdefABCDEF':
                    hexdigits += body[j]
                    j += 1
                out.append(int(hexdigits, 16) & 0xFF if hexdigits else 0)
                i = j
            elif nc in '01234567':
                j = i + 1
                k = 0
                val = 0
                while j < n and body[j] in '01234567' and k < 3:
                    val = val * 8 + int(body[j])
                    j += 1
                    k += 1
                out.append(val & 0xFF)
                i = j
            else:
                out.append(ord(nc) & 0xFF)
                i += 2
        else:
            out.append(ord(c) & 0xFF)
            i += 1
    return out


def rewrite(source):
    out = []
    i = 0
    n = len(source)
    while i < n:
        c = source[i]

        if c == '/' and i + 1 < n and source[i + 1] == '/':
            j = source.find('\n', i)
            j = n if j == -1 else j
            out.append(source[i:j])
            i = j
            continue

        if c == '/' and i + 1 < n and source[i + 1] == '*':
            j = source.find('*/', i + 2)
            j = n if j == -1 else j + 2
            out.append(source[i:j])
            i = j
            continue

        if c == "'":
            j = i + 1
            while j < n and source[j] != "'":
                j += 2 if source[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(source[i:j])
            i = j
            continue

        if c == '"':
            j = i + 1
            while j < n and source[j] != '"':
                j += 2 if source[j] == '\\' else 1
            j = min(j + 1, n)
            literal = source[i:j]
            body = literal[1:-1] if literal.endswith('"') else literal[1:]

            if body.startswith('\\p'):
                byte_values = decode_escapes(body[2:])
                if len(byte_values) > 255:
                    raise ValueError("Pascal string literal too long (>255 bytes): " + literal)
                vals = [str(len(byte_values))] + [str(b) for b in byte_values] + ['0']
                out.append('((unsigned char[]){' + ','.join(vals) + '})')
            else:
                out.append(literal)
            i = j
            continue

        out.append(c)
        i += 1

    return ''.join(out)


def main():
    if len(sys.argv) != 3:
        print("usage: pstr_rewrite.py <input.c> <output.c>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'r', encoding='utf-8', errors='surrogateescape') as f:
        src = f.read()

    out = rewrite(src)

    with open(sys.argv[2], 'w', encoding='utf-8', errors='surrogateescape') as f:
        f.write(out)


if __name__ == '__main__':
    main()
