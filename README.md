# FdtBusPkg - Device Tree-based Platform Device Driver Development

This repo implements support for developing Tiano platform device drivers
compliant to the UEFI Driver Model, by performing driver binding and
configuration using a Device Tree. Such a device tree is typically
either passed to UEFI by higher-priveleged firmware.

This is a staging branch created as part of the ongoing [RISE](https://riseproject.dev/) collaboration. The net goal is to upstream this code to Tiano.

Advantages:
- Allows UEFI developers to fully embrace modularity and code reuse.
- Facilitates development of complex (graphics, NIC, etc) drivers.
- Enables a single firmware binary to work across SoC revisions and
  board designs.

FdtBusPkg consists of FdtBusDxe, the DT-based bus driver that
implements EFI_DT_IO_PROTOCOL for enumerated devices, and a number
of examples drivers and libraries for demoing with the RISC-V
OVMF firmware.

FdtBusPkg components can be used on any architecture, but have been
developed and tested with RISC-V. They should be reusable out of the box
on AArch64 platforms as well, barring any missing dependencies.

Note: this is Device Tree being used internally by UEFI. There is no
relation to using Device Tree as possible mechanism of describing
hardware configuration to an OS.

See the [presentation slides](Docs/Uefi2023/slides.pdf) from the UEFI Fall 2023 Developers Conference and Plugfest.

## Updates

| When | What |
| :-: | ------------ |
| January 2024 | Open sourced. |
| October 2023 | Presented at the UEFI Fall 2023 Developers Conference and Plugfest. See the [presentation slides](Docs/Uefi2023/slides.pdf). |
| 2023 | Reported to RISE as a 2024 priority. |

## Quick Start

To build RISC-V OVMF firmware:

        $ git clone https://github.com/tianocore/edk2.git
        $ cd edk2
        $ git submodule add https://github.com/intel/FdtBusPkg
        $ git submodule update --init
        $ git am FdtBusPkg/Docs/0001-RiscVVirt-Patches-to-enable-FdtBusPkg-components.patch
        $ export GCC_RISCV64_PREFIX=... (if you are on a non-RISCV64 system)
        $ build -a RISCV64  -p OvmfPkg/RiscVVirt/RiscVVirtQemu.dsc -t GCC -b DEBUG

## License

FdtBusPkg is licensed under the BSD-2-Clause-Patent license (just like Tiano).

## Security Policy

Intel is committed to rapidly addressing security vulnerabilities affecting our customers and providing clear guidance on the solution, impact, severity and mitigation.

### Reporting a Vulnerability

Please report any security vulnerabilities in this project utilizing the guidelines [here](https://www.intel.com/content/www/us/en/security-center/vulnerability-handling-guidelines.html).

## Contribute

This is a [RISE Project](https://riseproject.dev) under the Firmware WG. See the [project wiki page](https://wiki.riseproject.dev/display/HOME/EDK2_00_03+-+FdtBusDxe+support).

Contributions are welcome. Please raise issues and pull requests.

Please see the [policy on contributions](CONTRIBUTING.md) and our [Code of Conduct](CODE_OF_CONDUCT.md).
