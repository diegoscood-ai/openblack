"""Paths the reverse-engineering tools need, from environment variables or a local config file.

Every setting is looked up in this order:
  1. the environment variable of the same name;
  2. tools/re/local_config.json, a flat {"NAME": "value"} object (gitignored: copy local_config.example.json);
  3. the tool's own default, if it has one.

Settings:
  RUNBLACK_EXE      your copy of the original game's runblack.exe (read only, never written)
  BW1_DECOMP        a clone of https://github.com/openblack/bw1-decomp (its symbols.txt names the exe's functions)
  BW1_SYMBOLS       the symbols file itself, if not <BW1_DECOMP>/config/BW1W120/symbols.txt
  ...               each tool's README lists its own optional settings

From a shell: python -I re_config.py get NAME [default]   prints the value, or the default, or nothing.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LOCAL = os.path.join(HERE, 'local_config.json')
_cache = None


def _local():
    global _cache
    if _cache is None:
        _cache = {}
        if os.path.isfile(LOCAL):
            with open(LOCAL, encoding='utf-8') as f:
                _cache = json.load(f)
    return _cache


def get(name, default=None):
    """The setting, or default when it is set nowhere."""
    v = os.environ.get(name)
    if v:
        return v
    v = _local().get(name)
    return v if v else default


def require(name, what):
    """The setting, or exit with a message saying how to set it."""
    v = get(name)
    if not v:
        sys.exit('%s is not set: %s.\nSet the environment variable, or add it to %s (see local_config.example.json).'
                 % (name, what, LOCAL))
    return v


def require_file(name, what):
    """A setting that must name an existing file."""
    v = require(name, what)
    if not os.path.isfile(v):
        sys.exit('%s=%s: no such file (%s).' % (name, v, what))
    return v


def runblack_exe():
    return require_file('RUNBLACK_EXE', 'the path of your copy of the original runblack.exe')


def symbols_file():
    """bw1-decomp's symbols.txt: BW1_SYMBOLS, else <BW1_DECOMP>/config/BW1W120/symbols.txt."""
    v = get('BW1_SYMBOLS')
    if not v:
        root = require('BW1_DECOMP', 'the path of a bw1-decomp clone (or set BW1_SYMBOLS to its symbols.txt)')
        v = os.path.join(root, 'config', 'BW1W120', 'symbols.txt')
    if not os.path.isfile(v):
        sys.exit('no symbols file at %s (set BW1_DECOMP to a bw1-decomp clone, or BW1_SYMBOLS).' % v)
    return v


if __name__ == '__main__':
    if len(sys.argv) in (3, 4) and sys.argv[1] == 'get':
        print(get(sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else '') or '')
        sys.exit(0)
    print(__doc__)
    sys.exit(2)
