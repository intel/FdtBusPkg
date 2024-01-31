# Another README for Developers

> [!NOTE]
> See [FdtBusPkg Documentation Style and Terms Definitions](StyleAndTerms.md) first.

## Preparing

### Additional Requirements

This is on top of the regular Tiano requirements covered elsewhere.

- cmake (for Uncrustify)
- device-tree-compiler (for the [regression test device tree](../Drivers/FdtBusDxe/TestDt.dts))

### Uncrustify

Uncrustify is a code beautifier used to format code into the [accepted Tiano style](https://tianocore-docs.github.io/edk2-CCodingStandardsSpecification/draft/).

Here are some simple steps to build and install Uncrustify:

        $ git clone https://projectmu@dev.azure.com/projectmu/Uncrustify/_git/Uncrustify
        $ cd Uncrustify
        $ mkdir build
        $ cd build
        $ cmake ..
        $ cmake -build .
        $ sudo make install

Here's a useful command:

        $ git ls-files *.h *.c | uncrustify -c $WORKSPACE/.pytool/Plugin/UncrustifyCheck/uncrustify.cfg -F - --replace --no-backup --if-changed

Also see https://github.com/tianocore/tianocore.github.io/wiki/EDK-II-Code-Formatting.

## Environment

Configuring a development environment is easy:

        $ git clone https://github.com/tianocore/edk2.git
        $ cd edk2
        $ git submodule add https://github.com/intel/FdtBusPkg
        $ git submodule update --init --recursive
        $ . edksetup.sh
        $ python3 BaseTools/Scripts/SetupGit.py
        $ make -C BaseTools

## Building

Once the environment is set up, to build RISC-V OVMF firmware enabled with FdtBusPkg components:

        $ git am FdtBusPkg/Docs/0001-RiscVVirt-Patches-to-enable-FdtBusPkg-components.patch
        $ export GCC_RISCV64_PREFIX=... (if you are on a non-RISCV64 system)
        $ build -a RISCV64  -p OvmfPkg/RiscVVirt/RiscVVirtQemu.dsc -t GCC -b DEBUG

## Notes

- Keep the docs in sync with any patches and changes to [DtIo.h](../Include/Protocol/DtIo.h).
- New functionality must be checked in along with the relevant regression test (in the same commit).
- Use Tiano coding style (run edk2 uncrustify).
