#!/usr/bin/env python3

import sys

symcache = {}
allocated = {}

def usage():
    print("Usage: leaks.py <filename>", file=sys.stderr)

def add_stack(addr, curstack):
    if addr in allocated:
        addr[allocated]["stack"] = curstack
    else:
        print("FATAL Could not find addr in allocated list", file=sys.stderr)

def process_symentry(line):
    words = line.split("-")
    symcache[words[1]] = words[2]

def process_free(line):
    ptr = line.split('(')[1].split(')')[0]
    if (ptr in allocated):
        del allocated[ptr]
        print("found %s in list" % ptr, file=sys.stderr)
    else:
        print("Could not find %s in allocated list" % ptr, file=sys.stderr)
    return

def process_malloc(line):
    size = line.split('(')[1].split(')')[0]
    ptr = line.split("=")[1].strip()
    entry = {}
    entry["size"] = size
    entry["stack"] = []
    allocated[ptr] = entry
    return

def process_realloc(line):
    return

def process_memalign(line):
    return

def process_call(line):
    if (line.startswith("+FREE")):
        process_free(line)
    elif (line.startswith("+MALLOC")):
        process_malloc(line)
    elif (line.startswith("+REALLOC")):
        process_realloc(line)
    elif (line.startswith("+MEMALIGN")):
        process_memalign(line)
    else:
        print("Should not have reached here", file=sys.stderr)
    return False, None

def process_file(f):
    curstack = []
    getstack = False
    addr = None
    lines = [line.strip() for line in f.readlines()]
    for line in lines:
        if (line.startswith("+")):
            getstack, addr = process_call(line)
            if (getstack):
                curstack = []
        elif (line.startswith("-")):
            process_symentry(line)
        elif (line.startswith("=")):
            print("Start/Stop marker")
        elif (line.startswith("0") and getstack):
            curstack.append(line)
        if (getstack and (len(curstack) > 0) and not line.startswith("0")):
            add_stack(addr, curstack)
            getstack = False
            curstack = []

if __name__ == "__main__":
    if (len(sys.argv) != 2):
        usage()
        sys.exit(1)
    with open(sys.argv[1], "r") as fd:
        process_file(fd)

