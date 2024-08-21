# FdtBusDxe Overview

> [!NOTE]
> See [FdtBusPkg Documentation Style and Terms Definitions](StyleAndTerms.md) first.

[FdtBusDxe](../Drivers/FdtBusDxe) is responsible for enumerating
DT controllers based on Devicetree nodes, and implementing
`EFI_DT_IO_PROTOCOL` for basic operations on such controllers, such as
device property access, register I/O, DMA buffer handling and child
device enumeration. It is overall meant as the logical successor for
the existing Tiano FdtClientDxe component.

## Entry

On loading, `EntryPoint()`:

- Uses FbpPlatformGetDt to get the platform Devicetree. The [default implementation](../Library/FbpPlatformDtLib) looks
  for the Devicetree HOB (`gFdtHobGuid`).
- On debug builds, check that the Devicetree is allocated in the UEFI memory map, i.e. not part of free memory.
- Bails if neither a platform Devicetree is available, nor regression tests are available (i.e. non-debug builds).
- Locates `EFI_CPU_IO2_PROTOCOL` (`gEfiCpuIo2ProtocolGuid` is in the `[Depex]` list)
- If a platform Devicetree is available, registers a notification callback on
`gEdkiiPlatformHasDeviceTreeGuid`, which is the "UEFI will expose
Devicetree to the OS" signal. The callback installs the managed
Devicetree in the EFI Configuration Table, which matches FdtClientDxe behavior.
- Registers an End-of-DXE notification callback. The callback ensures
that controllers [marked as critical](DtBindings.md#fdtbuspkgcritical)
are connected to their drivers and initialized when End-of-DXE is signaled
during the BDS phase.
- Registers as a bus driver, by:
  - Creating the device handle for the Devicetree root, if a platform Devicetree is found.
  - On debug builds, creating the device handle for the test Devicetree root (used for regression testing).
  - Registering the Driver Binding and Component Name protocols.

Unloading is not supported.

## Driver Binding

Every Devicetree node can be ultimately exposed as an `EFI_HANDLE` with the
`EFI_DT_IO_PROTOCOL` installed. Initially (ignoring the support for
regression testing), there's only the root Devicetree node. Its direct
children are enumerated by FdtBusDxe as part of the `Start()` Driver
Binding Protocol function being invoked.

FdtBusDxe manages the following Devicetree nodes:

- Devicetree root.
- _simple-bus_ (a simple container for devices).
- Regression (unit) testing nodes.

Every enumerated node is tracked via a `DT_DEVICE` structure.

Stopping is supported.

## Structure

| File | Description |
| ---- | ----------- |
| ComponentName.c | `EFI_COMPONENT_NAME_PROTOCOL` and `EFI_COMPONENT_NAME2_PROTOCOL`. |
| DriverBinding.c | `EFI_DRIVER_BINDING_PROTOCOL`. |
| DtDevice.c | `DT_DEVICE` object management. |
| DtIoProp.c | Simple `EFI_DT_IO_PROTOCOL` property API wrappers. |
| DtIoPropParse.c | Bulk of `EFI_DT_PROPERTY` management, property parsing and `EFI_DT_IO_PROTOCOL` property API. |
| DtIo.c | `EFI_DT_IO_PROTOCOL`. |
| Entry.c | Driver entrypoint and related. |
| Fdt.c | Simple wrappres around libfdt functionality. |
| Utils.c | Various. |
| Tests.c | Regression tests. |

## Testing

Debug builds come with automatic regression testing. This uses an
[entirely separate Devicetree](../Drivers/FdtBusDxe/TestDt.dts), and
is managed separately - there is a separate test Devicetree root, and
FdtBusDxe can easy differentiate a testing `DT_DEVICE` from a regular
one.

The test Devicetree [is built](../Drivers/FdtBusDxe/TestDt.sh) as part
of building `FdtBusPkg.dsc`. Both the source and the resulting TestDt.dtbi
file should be checked in, as the regular use of FdtBusPkg shouldn't
involve adding new regression tests. The test Devicetree is embedded
into the FdtBusDxe driver.

The nodes under `/unit-test-devices` are used for unit tests.
Others (like `/sample-device`) are for playing around with [sample
driver code](../Drivers/SampleDeviceDxe).

The regression test for each unit test device is invoked from
`DriverStart()`. The tests are contained in
[Tests.c](../Drivers/FdtBusDxe/Tests.c). Every unit test node
added to TestDt.dts must be declared in the `TestDescs[]` array.