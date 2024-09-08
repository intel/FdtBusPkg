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

```
$ git clone https://projectmu@dev.azure.com/projectmu/Uncrustify/_git/Uncrustify
$ cd Uncrustify
$ mkdir build
$ cd build
$ cmake ..
$ cmake -build .
$ sudo make install
```

Here's a useful command to run the styler on all C headers and sources under the current directly.

```
$ git ls-files *.h *.c | uncrustify -c $WORKSPACE/.pytool/Plugin/UncrustifyCheck/uncrustify.cfg -F - --replace --no-backup --if-changed
```

Also see https://github.com/tianocore/tianocore.github.io/wiki/EDK-II-Code-Formatting.

## Environment

Configuring a development environment is easy:

```
$ git clone https://github.com/tianocore/edk2.git
$ cd edk2
$ git submodule add https://github.com/intel/FdtBusPkg
$ git submodule update --init --recursive
$ . edksetup.sh
$ python3 BaseTools/Scripts/SetupGit.py
$ git am FdtBusPkg/Docs/edk2-patches/*
$ pushd FdtBusPkg
$ python3 ../BaseTools/Scripts/SetupGit.py
$ popd
$ make -C BaseTools
```

## Building

Once the environment is set up, to build all of the components (outside of a firmware build):

```
$ export GCC_RISCV64_PREFIX=... (if you are on a non-RISCV64 system)
$ build -a RISCV64 -t GCC5 -p FdtBusPkg/FdtBusPkg.dsc -b DEBUG
```

To build RISC-V OVMF firmware enabled with FdtBusPkg components:

```
$ git am FdtBusPkg/Docs/ovmf-patches/*
$ export GCC_RISCV64_PREFIX=... (if you are on a non-RISCV64 system)
$ build -a RISCV64  -p OvmfPkg/RiscVVirt/RiscVVirtQemu.dsc -t GCC -b DEBUG
```

## Notes

- Keep the docs in sync with any patches and changes to [DtIo.h](../Include/Protocol/DtIo.h).
- Keeps the [test applications](../Application) in sync with any patches and changes to [DtIo.h](../Include/Protocol/DtIo.h).
- New functionality must be checked in along with the relevant regression test (in the same commit).
- Use Tiano coding style (run edk2 uncrustify).
- Regression tests should avoid unnecessary logging and test conditionals. `ASSERT` is your friend.
- Checking in regression tests means checking in TestDt.dts **and** TestDt.dtbi changes.
- When adding new components, don't forget to add them to [FdtBusPkg.dsc](../FdtBusPkg.dsc) and test the build.
- When adding new test applications, don't forget to add them to [FdtBusPkgApps.dsc](../FdtBusPkgApps.dsc) and test the build.

## Testing

A debug build of FdtBusDxe comes with a regression test suite. This
runs automatically and will crash the firmware on failure. You don't
need to take any special steps.

To build test apps:

```
$ export GCC_RISCV64_PREFIX=... (if you are on a non-RISCV64 system)
$ build -a RISCV64 -t GCC5 -p FdtBusPkg/FdtBusPkgApps.dsc -b DEBUG
```

### DtInfo.efi

Dumps `EFI_DT_IO_PROTOCOL` info for a DT controller.

#### Usage

```
Shell> FS0:\DtInfo controller
```

#### Parameters

* `controller`: a hex `EFI_HANDLE`, a hex device handle index (from
`devtree`), an alias or absolute DT path.

> [!NOTE]
> A lookup by path will connect any missing drivers and enumerate missing devices along the path.

> [!CAUTION]
> The unit address portion of the DT path may not be omitted under any circumstances. Passing "/soc/pci@30000000" is okay but "/soc/pci" is not. This is a restriction in the current implementation and a difference in behavior, when compared to the [Devicetree Specification, Section 2.2.3](https://devicetree-specification.readthedocs.io/en/stable/devicetree-basics.html#path-names).

#### Examples

```
Shell> FS0:\DtInfo 17F44B918
Shell> FS0:\DtInfo 99
Shell> FS0:\DtInfo soc/pci@30000000
Shell> FS0:\DtInfo /soc/pci@30000000
```

Here's an example of output:
```
     ComponentName: 'DT(serial@10000000)'
              Name: 'serial@10000000'
        DeviceType: '[NONE]'
      DeviceStatus: 'OKAY'
      AddressCells: '2'
         SizeCells: '2'
 ChildAddressCells: '0'
    ChildSizeCells: '0'
     IsDmaCoherent: 'no'
      ParentDevice: '17F447E18'
        Compatible: 'ns16550a'
               Reg: #0 10000000(100) MemoryMappedIo UC
```

### DtProp.efi

Dumps a property value for a DT controller.

#### Usage

```
Shell> FS0:\DtProp controller property [parse string]
```

#### Parameters

* `controller`: a hex `EFI_HANDLE`, a hex device handle index (from
`devtree`), an alias or absolute DT path. Just like [DtInfo.efi](#dtinfoefi).
* `property`: property name to dump.
* `parse string`: commands to parse the property data.

If the optional `parse string` is not provided, the tool performs
a hex dump of the `property` data.

The `parse string`, when provided, contains single-character commands to
parse the `property` data. This enables parsing complex and arbitrary
formats.

| Command | Parse Type |
| --------| ----------- |
| `1`     | `EFI_DT_VALUE_U32` |
| `2`     | `EFI_DT_VALUE_U64` |
| `4`     | `EFI_DT_VALUE_U128` |
| `b`     | `EFI_DT_VALUE_BUS_ADDRESS` |
| `B`     | `EFI_DT_VALUE_CHILD_BUS_ADDRESS` |
| `z`     | `EFI_DT_VALUE_SIZE` |
| `Z`     | `EFI_DT_VALUE_CHILD_SIZE` |
| `r`     | `EFI_DT_VALUE_REG` |
| `R`     | `EFI_DT_VALUE_RANGE` |
| `s`     | `EFI_DT_VALUE_STRING` |
| `d`     | `EFI_DT_VALUE_DEVICE` |

#### Examples

Hex dump of a property:
```
Shell> FS0:\DtProp /soc/pci@30000000 compatible
Dumping 22 bytes of 'compatible':
  00000000: 70 63 69 2D 68 6F 73 74-2D 65 63 61 6D 2D 67 65  *pci-host-ecam-ge*
  00000010: 6E 65 72 69 63 00                                *neric.*
```

An empty property:
```
Shell> FS0:\DtProp /soc/pci@30000000 dma-coherent
Property 'dma-coherent' exists but is EMPTY
````

Formatting property data using the parse string parameter:
```
Shell> FS0:\DtProp /soc/pci@30000000 reg 1111
Parsing 'reg' with command string '1111':
00000000: 0x0
00000004: 0x30000000
00000008: 0x0
0000000C: 0x10000000
Shell> FS0:\DtProp /soc/pci@30000000 reg bz
Parsing 'reg' with command string 'bz':
00000000: 30000000
00000008: 10000000
Shell> FS0:\DtProp /soc/pci@30000000 reg r
Parsing 'reg' with command string 'r':
00000000: 30000000(10000000) MemoryMappedIo UC
Shell> FS0:\DtProp /soc/pci@30000000 ranges RRR
Parsing 'ranges' with command string 'RRR':
00000000: 0x10000000000000000000000(10000)->0x3000000 MemoryMappedIo UC
0000001C: 0x20000000000000040000000(40000000)->0x40000000 MemoryMappedIo UC
00000038: 0x30000000000000400000000(400000000)->0x400000000 MemoryMappedIo UC
Shell> FS0:\DtProp sample-bus/sample-device@1337 reg r
Parsing 'reg' with command string 'r':
00000000: 1337(100) via DT(sample-bus)
```

The first column in the output is the offset of the parsed element within the
property data.

### DtReg.efi

Reads and writes DT controller register regions.

#### Usage

```
Shell> FS0:\DtReg [-i reg index|name] [-n count] [-w access width] controller offset [set value]
```

#### Parameters

* `-i`: _reg_ index or name. When not provided, the first (index 0) region is used.
* `-n`: number of reads or writes to perform. When not provided, 1 is used.
* `-w`: access width (1, 2, 4 or 8). When not provded, 1 is used.
* `controller`: a hex `EFI_HANDLE`, a hex device handle index (from
`devtree`), an alias or absolute DT path. Just like [DtInfo.efi](#dtinfoefi).
* `offset`: a region offset (decimal or hex).
* `set value`: when provided, the value to write to register region.

If the optional `set value` parameter is not provided, the tool reads
from a register region. If the `set value` parameter is provided, to
tool writes to a register region. The accesses begin at the specified
`offset`, with the address incrementing `count` times.

#### Examples

4 reads of 8 bytes each at offset 8:
```
FS0:\> DtReg -n 4 -w 8 /sample-bus/sample-device@1337 8
Dumping 32 bytes at offset 0x8 of reg via DT(sample-bus) 0x1337(100):
00000008: 8888888888888808
00000010: 8888888888888810
00000018: 8888888888888818
00000020: 8888888888888820
```

Write 1 byte at offset 0:
```
Shell> FS0:\DtReg soc/serial@10000000 0 41
```

### PciInfo.efi

Tool for dumping BAR info for PCI devices or a specific PCI device.

This might seem a strange tool to have in FdtBusPkg, but it
is very useful in diagnosing PciHostBridgeFdtDxe, as it
shows BARs and both bus- and CPU-side addresses.

#### Usage

```
Shell> FS0:\pciinfo.EFI seg bus dev func
Shell> FS0:\pciinfo.EFI handle
Shell> FS0:\pciinfo.EFI [-v]
```

#### Parameters

* `seg`: a hex segment number.
* `bus`: a hex bus number.
* `dev`: a hex device number.
* `func`: a hex function number.
* `handle`: a hex `EFI_HANDLE`, a hex device handle index (from `devtree`).
* `-v`: verbose, dumping info for all PCI devices when a device is not specified.

#### Examples

Dump info about a given handle index (from `devtree` output):

```
Shell> PciInfo bd
[BD] 0000:00:03.00 info:
-------------------------
     Vendor: 1AF4 Device: 1000
  Supported: IO MEM BM ED ER DAC
    Current: IO MEM BM
       ROMs:
+0x00000000: BIOS (0x0000) image (0x10E00 bytes)
+0x00010E00: UEFI (0x8664) image (0x16600 bytes)
+0x00010E00:    Subsystem: 0xB
+0x00010E00:    InitializationSize: 0x16600 (bytes)
+0x00010E00:    EfiImageHeaderOffset: 0x38
+0x00010E00:    Compressed: yes
       BAR0: IO    CPU 0x0000000003000000 -> PCI 0x0000000000000000 (0x20)
       BAR1: MEM32 CPU 0x0000000040044000 -> PCI 0x0000000040044000 (0x1000)
```

## FAQ

### How do I load a standalone built driver?

That is, what if FdtBusDxe.efi is not part of my firmware?

Put FdtBusDxe.efi on removable media and use the `load` Shell command:

```
Shell> fs0:
Shell> load FdtBusDxe.efi
```

### How do I know the FdtBusDxe driver is present?

Use the `drivers` Shell command.

```
Shell> drivers
            T   D
D           Y C I
R           P F A
V  VERSION  E G G #D #C DRIVER NAME                         IMAGE NAME
== ======== = = = == == =================================== ==========
...
27 0000000A B - - 28 19 Devicetree Bus Driver               FdtBusDxe
```

The driver will load if it is not able to locate the Devicetree blob via FDT HOB, but the driver was built with regression test support.

### How do I know the FdtBusDxe driver successfully loaded?

Use the `devtree` Shell command.

```
Shell> devtree
...
 Ctrl[28] DT(DtRoot)
...
 Ctrl[29] DT(DtTestRoot)
   Ctrl[4B] DT(aliases)
   Ctrl[4C] DT(G0)
   Ctrl[4D] DT(G1)
   Ctrl[4E] DT(G2)
```

The `DT(DtTestRoot)` device will be present if FdtBusDxe was build with regression testing support (i.e. if it were a DEBUG build).

### Can I stop or disconnect FdtBusDxe?

Yes.

> [!CAUTION]
> If your console device is a DT controller, this will disconnect it too.

> [!NOTE]
> If you run `reconnect -r`, the `devtree` output from the Shell will be
> stale. Simply exit the Shell and run it again.

### Can I unload FdtBusDxe?

Not today.

### How can I tell how/if a DT controller is managed?

Using `devtree` and `dh` tools.

First, look at `devtree` and note the device handle index and
component name. If component name is `Legacy-Managed Device`, then the
DT controller is managed by a legacy driver. That is, it is managed by
code that doesn't conform to the UEFI Driver Mode. See the
[documentation on device drivers](DeviceDrivers.md) for more info.

```
Shell> devtree
...
 Ctrl[28] DT(DtRoot)
   Ctrl[2A] Legacy-Managed Device
   Ctrl[2B] DT(soc)
     Ctrl[2C] Legacy-Managed Device
     Ctrl[97] DT(pmu)
     Ctrl[98] DT(rtc@101000)
     Ctrl[99] 16550 UART Device
       Ctrl[A5] FDT Serial Port #0
         Ctrl[A6] PC-ANSI Serial Console
           Ctrl[6E] Primary Console Input Device
           Ctrl[6F] Primary Console Output Device
           Ctrl[70] Primary Standard Error Device
```

If a device has a component name of the form `DT(...)`, then it is
either not managed by any driver or is managed by FdtBusDxe itself.
In general, this is where the `dh` tool helps.

In the following example, we see `DT(soc)` is managed by FdtBusDxe,
which makes sense as it is a _simple-bus_ container device:

```
2B: DevicePath(..-1EE3-425E3650A29B,736F6300)) 5CE5A2B0-2838-3C35-1EE3-425E3650A29B
   Controller Name    : DT(soc)
   Device Path        : VenHw(5CE5A2B0-2838-3C35-1EE3-425E3650A29B,4474526F6F7400)/VenHw(5CE5A2B0-2838-3C35-1EE3-425E3650A29B,736F6300)
   Controller Type    : BUS
   Configuration      : NO
   Diagnostics        : NO
   Managed by         :
     Drv[27]          : Devicetree Bus Driver
   Parent Controllers :
     Parent[28]       : DT(DtRoot)
   Child Controllers  :
     Child[2C]        : Legacy-Managed Device
     Child[97]        : DT(pmu)
     Child[98]        : DT(rtc@101000)
     Child[99]        : 16550 UART Device
     Child[9A]        : DT(test@100000)
     Child[9B]        : DT(virtio_mmio@10008000)
     Child[9C]        : DT(virtio_mmio@10007000)
     Child[9D]        : DT(virtio_mmio@10006000)
     Child[9E]        : DT(virtio_mmio@10005000)
     Child[9F]        : DT(virtio_mmio@10004000)
     Child[A0]        : DT(virtio_mmio@10003000)
     Child[A1]        : DT(virtio_mmio@10002000)
     Child[A2]        : DT(virtio_mmio@10001000)
     Child[A3]        : DT(plic@c000000)
     Child[A4]        : DT(clint@2000000)
```

Here we see that `DT(rtc@101000)` is not managed by a UEFI Driver
Model-compliant device driver. Most likely, it is not used by any
driver (at least one following the [documentation on device
drivers](DeviceDrivers.md)):

```
98: 5CE5A2B0-2838-3C35-1EE3-425E3650A29B DevicePath(..A29B,7274634031303130303000))
   Controller Name    : DT(rtc@101000)
   Device Path        : VenHw(5CE5A2B0-2838-3C35-1EE3-425E3650A29B,4474526F6F7400)/VenHw(5CE5A2B0-2838-3C35-1EE3-425E3650A29B,736F6300)/VenHw(5CE5A2B0-2838-3C35-1EE3-425E3650A29B,7274634031303130303000)
   Controller Type    : DEVICE
   Configuration      : NO
   Diagnostics        : NO
   Managed by         : <None>
   Parent Controllers :
     Parent[2B]       : DT(soc)
   Child Controllers  : <None>
```

Here we see that the device with handle index `99` is managed by a `16550
UART Driver`. The component name is not in the form `DT(...)`,
because the UART driver provides its own component naming for the devices
it manages.

```
Shell> dh -d 99
99: 5CE5A2B0-2838-3C35-1EE3-425E3650A29B DevicePath(..269616C40313030303030303000))
   Controller Name    : 16550 UART Device
   Device Path        : VenHw(5CE5A2B0-2838-3C35-1EE3-425E3650A29B,4474526F6F7400)/VenHw(5CE5A2B0-2838-3C35-1EE3-425E3650A29B,736F6300)/VenHw(5CE5A2B0-2838-3C35-1EE3-425E3650A29B,73657269616C40313030303030303000)
   Controller Type    : BUS
   Configuration      : NO
   Diagnostics        : NO
   Managed by         :
     Drv[73]          : 16550 UART Driver
   Parent Controllers :
     Parent[2B]       : DT(soc)
   Child Controllers  :
     Child[A5]        : FDT Serial Port #0
```

The [DtInfo.efi](#dtinfoefi) tool will list more info about this
device handle, including the FdtBusDxe component name:

```
Shell> FS0:\DtInfo 99
     ComponentName: 'DT(serial@10000000)'
              Name: 'serial@10000000'
...
```
