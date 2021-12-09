# qemu-coverage
QEMU plugin demonstrating code coverage.  Each block of code being 
   executed is listed alongside with the contents of the block itself.

Coverage is gathered during either program or system execution, outputted when finished.

Code Block executions: 7 (# of instructions: 2)
--- 0x40018e217c: "testq %r14, %r14"
--- 0x40018e217f: "je 0x40018e2223"

Code Block executions: 4 (# of instructions: 5)
--- 0x40018e2185: "movq 0xd8(%r15), %rax"
--- 0x40018e218c: "movq %rax, %rdx"
--- 0x40018e218f: "subq %r12, %rdx"
--- 0x40018e2192: "cmpq %rdx, %rbp"
--- 0x40018e2195: "jbe 0x40018e21f0"

Credits: https://qemu.readthedocs.io/en/latest/devel/tcg-plugins.html

Copyright (C) 2021, Steven Wirsz <swirsz@gmail.com>

License: GNU GPL, version 2 or later.
