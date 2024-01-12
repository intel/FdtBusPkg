# SSE UEFI Package

## How to build?

```
#!/bin/bash
set -e
echo `pwd`
. edksetup.sh
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-
export GCC5_RISCV64_PREFIX=riscv64-linux-gnu-

build -a AARCH64 -t GCC5 -p FdtBusPkg/FdtBusPkg.dsc -b DEBUG
build -a RISCV64 -t GCC5 -p FdtBusPkg/FdtBusPkg.dsc -b DEBUG
```