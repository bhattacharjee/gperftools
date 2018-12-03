#!/usr/bin/env python3

import sys
import json

symcache = {}
allocated = {}
aggregated = {}


def usage():
    print("Usage: leaks.py <filename>", file=sys.stderr)

def getStackHash(stack):
    if (len(stack) == 0):
        return None
    return "".join(stack)

def add_stack(addr, curstack):
    if addr in allocated:
        entry = allocated[addr]
        entry["stack"] = curstack
        del allocated[addr]
        allocated[addr] = entry
    else:
        print("FATAL Could not find addr in allocated list",
                addr, file=sys.stderr)

def process_symentry(line):
    words = line.split("-")
    symcache[words[1]] = words[2]

def process_free(line):
    ptr = line.split('(')[1].split(')')[0]
    if (ptr in allocated):
        del allocated[ptr]
    else:
        print("Could not find %s in allocated list" % ptr, file=sys.stderr)
    return False, ptr

def process_malloc(line):
    size = line.split('(')[1].split(')')[0]
    ptr = line.split("=")[1].strip()
    entry = {}
    entry["size"] = size
    entry["stack"] = []
    allocated[ptr] = entry
    return True, ptr

def process_realloc(line):
    words1 = line.split('(')[1].split(')')[0].split(",")
    oldptr = words1[0].strip()
    newsize = words1[1].strip()
    newptr = line.split("=")[1].strip()
    if (oldptr in allocated):
        del allocated[oldptr]
    else:
        print("Could not find %s in allocated list (REALLOC)" % ptr,
                file=sys.stderr)
    entry = {}
    entry["size"] = newsize
    entry["stack"] = []
    allocated[newptr] = entry
    print("1. overwrite stack for ", ptr)
    return True, newptr

def process_memalign(line):
    size = line.split('(')[1].split(')')[0].split(",")[0].strip()
    ptr = line.split("=")[1].strip()
    entry = {}
    entry["size"] = size
    entry["stack"] = []
    allocated[ptr] = entry
    return True, ptr

def process_call(line):
    if (line.startswith("+FREE")):
        return process_free(line)
    elif (line.startswith("+MALLOC")):
        return process_malloc(line)
    elif (line.startswith("+REALLOC")):
        return process_realloc(line)
    elif (line.startswith("+MEMALIGN")):
        return process_memalign(line)
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
            pass
        elif (line.startswith("0") and getstack):
            curstack.append(line)
        if (getstack and (len(curstack) > 0) and not line.startswith("0")):
            add_stack(addr, curstack)
            getstack = False
            curstack = []

def aggregate():
    for ptr, entry in allocated.items():
        aggrentry = {}
        aggrentry["tot_size"] = 0
        aggrentry["n_alloc"] = 0
        sthash = getStackHash(entry["stack"])
        if (sthash == None):
            print("Could not get stack for ",
                    ptr,
                    "".join(entry["stack"]),
                    entry["size"],
                    file=sys.stderr)
            continue
        if sthash in aggregated:
            aggrentry = aggregated[sthash]
        aggrentry["tot_size"] = aggrentry["tot_size"] + int(entry["size"])
        aggrentry["n_alloc"] = aggrentry["n_alloc"] + 1
        aggrentry["stack"] = entry["stack"]
        aggregated[sthash] = aggrentry

def printresult():
    for sthash, entry in aggregated.items():
        print("=" * 80)
        print("%d bytes lost in %d allocations, avg = %d" %
            (entry["tot_size"],
            entry["n_alloc"],
            entry["tot_size"] / entry["n_alloc"]))
        print("-" * 80)
        for frame in entry["stack"]:
            if frame in symcache:
                print(symcache[frame])
            else:
                print(frame)
    print("=" * 80)

if __name__ == "__main__":
    if (len(sys.argv) != 2):
        usage()
        sys.exit(1)
    with open(sys.argv[1], "r") as fd:
        process_file(fd)
        aggregate()
        printresult()

