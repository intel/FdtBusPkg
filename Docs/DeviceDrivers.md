# Devicetree Device Drivers

> [!NOTE]
> See [FdtBusPkg Documentation Style and Terms Definitions](StyleAndTerms.md) first.

DT (Devicetree) device drivers manage DT controllers.  Device handles
for supported DT controllers are created by a Devicetree bus driver
(e.g. FdtBusDxe).

There are two approaches to writing such drivers. The preferred
mechanism is to follow the UEFI Driver Model by implementing
driver binding. The alternative approach (called legacy in this
document) may be suitable under some circumstances.

See [HighMemDxe](../Drivers/HighMemDxe) for an example of a driver
that can be compiled as either a UEFI Driver Model driver or a legacy driver.

## UEFI Driver Model

A DT device driver following the UEFI Driver Model does so by
installing an `EFI_DRIVER_BINDING_PROTOCOL` on the the driver image
handle. The UEFI driver dispatch logic will then use installed
protocol to match device drivers against available device handles.

A DT device driver typically does not create any new
device handles. Instead, it attaches a protocol instance to the device
handle of the DT controller. These protocol instances are I/O
abstractions that allow the DT controller to be used in the preboot
environment. The most common I/O abstractions are used to boot an EFI
compliant OS.

The following figure shows the device handle for a DT controller
before and after `Start()` Driver Binding Protocol function is
called. In this example, a DT device driver is adding the Block I/O
Protocol to the device handle for the DT controller.

```mermaid
stateDiagram-v2

    classDef Gray fill:grey,color:white;

    state "DT Controller Device Handle" as C1 {
        EFI_DEVICE_PATH_PROTOCOL1: EFI_DEVICE_PATH_PROTOCOL
        EFI_DT_IO_PROTOCOL1: EFI_DT_IO_PROTOCOL
    }

    state "DT Controller Device Handle" as C2 {
        EFI_DEVICE_PATH_PROTOCOL2: EFI_DEVICE_PATH_PROTOCOL
        EFI_DT_IO_PROTOCOL2: EFI_DT_IO_PROTOCOL
        EFI_BLOCK_IO_PROTOCOL

        note left of EFI_BLOCK_IO_PROTOCOL: Installed by Start()\nUninstalled by Stop()
    }

    C1 --> C2: Start() opens DT I/O
    C2 --> C1: Stop() closes DT I/O

    class EFI_BLOCK_IO_PROTOCOL Gray
```
### Driver Binding Protocol for DT Device Drivers

The Driver Binding Protocol contains three services. These are
`Supported()`, `Start()`, and `Stop()`.

#### `Supported()`

`Supported()` tests to see if the DT
device driver can manage a device handle. A DT device driver can only
manage device handles that contain the Device Path Protocol and the
Devicetree I/O Protocol, so a DT device driver must look for these two
protocols on the device handle that is being tested.

> [!WARNING]
> Keep it mind that `OpenProtocol()` Boot Service may return
> `EFI_ALREADY_STARTED`, which should be handled like `EFI_SUCCESS`.

In addition, the `Supported()` function needs to check if the DT controller
can be managed. This is typically done by using Devicetree I/O Protocol
functions to check against supported _compatible_ (identification) and
other expected property values (such as device status):

```
BOOLEAN Supported;

Supported = !EFI_ERROR (DtIo->IsCompatible (DtIo, "device-compat-string")) &&
            DtIo->DeviceStatus == EFI_DT_STATUS_OKAY;
```

#### `Start()`

The `Start()` function tells the DT device driver to start managing a
DT controller. First, the `Start()` function needs to use the `OpenProtocol()`
Boot Service with `BY_DRIVER` to open the relevant protocols, like the DT
 I/O Protocol.

> [!WARNING]
> Keep it mind that `OpenProtocol()` may return `EFI_ALREADY_STARTED`,
> which should be handled like `EFI_SUCCESS` with some caveats.
> See [VirtioFdtDxe](../Drivers/VirtioFdtDxe/DriverBinding.c#L416) for a simple example.

A DT device driver typically does not create any new
device handles. Instead, it installs one or more additional protocol
instances on the device handle for the DT controller.

#### `Stop()`

The `Stop()` function mirrors the `Start()` function, so the `Stop()`
function completes any outstanding transactions to the DT controller
and removes the protocol interfaces that were installed in
`Start()`.

### DT Controllers with Children

Some DT device drivers may need to create new device handles.

[VirtioFdtDxe](../Drivers/VirtioFdtDxe) is a good example of a DT
controller device driver creating a new device handle that is NOT
a DT controller.

In some situations, a DT controller's children are actually DT
controllers that need to be enumerated.  A good example may be
supporting a Devicetree node for a composite device
such as a NIC or graphics.

Let's examine a Devicetree snippet:

```
genet: ethernet@7d580000 {
       compatible = "brcm,bcm2711-genet-v5";
       ...

       genet_mdio: mdio@e14 {
                   compatible = "brcm,genet-mdio-v5";
                   ...

                   phy1: ethernet-phy@1 {
                         compatible = "phy-driver-compat-string";
                         reg = <0x1>;
                   }
       };
};
```

In this example, the NIC driver would bind to the `genet` device and
enumerate the MDIO device (which would probably bind to the same
NIC driver). In the context of the MDIO device, it would then
enumerate the `phy1` device, and continue initialization
once a PHY driver loads and publishes an interface.

> [!TIP]
> How could a driver managing a DT controller and its children
> be able to relate a DT controller `EFI_HANDLE` to another
> another device being managed? You can use the `ParentDevice`
> structure field in `EFI_DT_IO_PROTOCOL`.

Such drivers have additional `EFI_DT_IO_PROTOCOL`-specific
operations they need to perform in their `Start()` and `Stop()` Driver
Binding Protocol functions.

> [!NOTE]
> You might wonder why these child DT controllers cannot all be
> automatically enumerated by FdtBusDxe. That would imply
> FdtBusDxe would be managing every DT controller, which would
> prevent any other driver from starting on the created DT handles!
> FdtBusDxe binds to a very small set of DT controller types.

#### `Start()`

DT device drivers that need to enumerate further child DT controllers
can do so via the `ScanChildren()` Devicetree I/O Protocol function. A
driver has the option of creating all of its children in one call to
`Start()`, or spreading it across several calls to `Start()`. In
general, if it is possible to design a driver to create one child at a
time (e.g. the child is not some intrinsic criticial component of the
device), it should do so to support the rapid boot capability in the
UEFI Driver  Model. DT device drivers enumerating child DT controllers
may also register callback via the `SetCallbacks()` Devicetree I/O
Protocol function, to directly handle child register reads and writes.

#### `Stop()`

If the DT device driver enumerated further child DT
controllers, these need to be cleaned up via the `RemoveChild()`
Devicetree I/O Protocol function. If DT bus driver callbacks were
registered, these must be unregistered via an appropriate `SetCallbacks()`
Devicetree I/O Protocol function call.

## Legacy Drivers

Legacy drivers are generally outside the realm of bus-enumerated
device handles. These typically directly publish a protocol and
hardcode the device details. However, there may be objective reasons
why a driver makes use of a device handle with an `EFI_DT_IO_PROTOCOL`
but bypasses driver binding.

Tiano has a notion of "library drivers". For example, there's a
generic Serial DXE driver, where the actual hardware interaction is
encapsulated entirely by SerialPortLib, where the SerialPortLib
interface is generic enough for a library to be linked to SEC, PEI, DXE
or even MM images. Similarly, PciHostBridgeDxe relies on
PciHostBridgeLib for discovery of PCIe RC information. These
drivers do not publish an `EFI_DRIVER_BINDING_PROTOCOL`. In
an environment with DT controllers, they rely on libraries to
fully encapsulate any discovery and interaction with
`EFI_DT_IO_PROTOCOL`-bearing device handles.

### Simple Example

The following figure demonstrates how a legacy driver or library can locate
supported DT controllers.

```mermaid
stateDiagram-v2

C1: HandleBuffer = LocateHandleBuffer(gEfiDtIoProtocolGuid)

state "Foreach EFI_HANDLE in HandleBuffer" as C2 {
    D1: DtIo = OpenProtocol (Handle, BY_DRIVER)
    D2: DtIo->IsCompatible ("device-compat-string")
    D3: DtIo->DeviceStatus == EFI_DT_STATUS_OKAY
    D5: CloseProtocol (Handle)
    D4: ProcessController (Handle)
}

C1 --> C2
C2 --> C3
D1 --> D2: EFI_SUCCESS
D2 --> D3: Yes
D2 --> D5: No
D3 --> D4: Yes
D3 --> D5: No
D4 --> D5

C3: FreePool(HandleBuffer)
```
The steps are:

- Call the `LocateHandleBuffer` UEFI Boot Service with the
  `gEfiDtIoProtocolGuid`.
- For every handle:
  - Locate the DT I/O Protocol on the handle.
    - If the controller is meant to be exclusively used by the driver or library
      (e.g. talking to hardware),  call `OpenProtocol()` Boot Service with `BY_DRIVER`
      to get the DT I/O Protocol. This ensures the driver doesn't start using a
      controller that is already managed by another driver. It also ensures that
      other (well behaved) drivers won't use the controller until it is released.
    - If the resource is meant to be shared with other components (e.g. SerialPortLib),
      or if the driver or library simply wants to query some information about the controller
      (i.e. not talk to hardware), use `HandleProtocol()`.
  - Use the `IsCompatible()` Devicetree I/O Protocol call to identify supported controllers.
  - Filter out controllers with `DtIo->DeviceStatus != EFI_DT_STATUS_OKAY`.
  - Call `CloseProtocol()` Boot Service on unsupported controllers if `OpenProtocol()` was used.
- Free handle buffer.

> [!CAUTION]
> Failing to close unsupported controllers will result in other
> drivers not being able to start on their device handles!

See [PciHostBridgeLibEcam](../Library/PciHostBridgeLibEcam) for an example.

This library has a dependency on `gEfiDtIoProtocolGuid`,
as the supported controllers are expected to be enumerated once
FdtBusDxe loads, so the availability of a DT controller present in the system is sufficient.

PciHostBridgeLibEcam only has a single user - PciHostBridgeLibEcam,
so the DT I/O protocol is located using `OpenProtocol` with `BY_DRIVER`.

It's easy to identify DT controllers that are managed by a legacy
driver that uses `OpenProtocol()` as suggested. These are listed as `Legacy-Managed Device`:

```
Shell> devtree
...
 Ctrl[2A] DT(DtRoot)
   Ctrl[2C] DT(reserved-memory)
   Ctrl[2D] DT(fw-cfg@10100000)
   Ctrl[2E] DT(flash@20000000)
   Ctrl[2F] DT(chosen)
   Ctrl[30] DT(poweroff)
   Ctrl[31] DT(reboot)
   Ctrl[32] DT(platform-bus@4000000)
   Ctrl[33] Legacy-Managed Device
   Ctrl[34] DT(cpus)
...
```

### More Complex Example

The following figure demonstrates supporting controllers enumerated after the legacy driver loads. Some controllers are handled immediately, while
other ones are picked up in the future as they are enumerated.

```mermaid
stateDiagram-v2

C1: HandleBuffer = LocateHandleBuffer(gEfiDtIoProtocolGuid)

state "Process Handle" as C3 {
    D1: DtIo = OpenProtocol (Handle, BY_DRIVER)
    D2: DtIo->IsCompatible ("device-compat-string")
    D3: DtIo->DeviceStatus == EFI_DT_STATUS_OKAY
    D5: CloseProtocol (Handle)
    D4: ProcessController (Handle)
}

state "Foreach EFI_HANDLE in HandleBuffer" as C2 {
    C2P0: Process Handle
}

state "Register DT I/O Notification Callback" as C5 {
    E1: Event = CreateEvent (DtIoInstallCallback)
    E2: RegisterProtocolNotify (gEfiDtIoProtocolGuid, Event)
}

state "DtIoInstallCallback" as C6 {
    C6P0: Process Handle

}

C1 --> C2: EFI_SUCCESS
C1 --> C5: EFI_NOT_FOUND
D1 --> D2: EFI_SUCCESS
D2 --> D3: Yes
D2 --> D5: No
D3 --> D4: Yes
D3 --> D5: No
D4 --> D5

C2P0 --> C3
C6P0 --> C3


E1 --> E2: EFI_SUCCESS

C2 --> C4
C4: FreePool(HandleBuffer)
```

The steps are:
- Register a protocol notification callback on `gEfiDtIoProtocolGuid`.
- Identify the DT controller handles inside the notification callback via `LocateHandle` Boot Service using `ByRegisterNotify`.
- Locate the DT I/O Protocol as appropriate.
- Close the notification callback event if no more DT controllers are expected.

See [FdtPciPcdProducerLib](../Library/FdtPciPcdProducerLib) for an example.

This library is linked into a number of drivers,
including CpuDxe. The latter is a dependency for FdtBusDxe (as it
publishes `EFI_CPU_IO2_PROTOCOL`), so it is not possible to use a
`[Depex]` dependency on `gEfiDtIoProtocolGuid`. Instead, if
`LocateHandleBuffer` fails because the library is used before
FdtBusDxe is loads, a protocol notification callback
is set.

Because this library is linked into a number of drivers, and because
its interaction with the DT controller is limited to querying a few
properties about it, the DT I/O Protocol is located using `HandleProtocol`
and not `OpenProtocol`.

> [!CAUTION]
> If allocating resources in a library, don't forget to clean these
> up in a destructor function. Failure to close events in a library
> will cause crashes when a callback is invoked in an unloaded driver!

Yes, legacy drivers are awkward and messy. This is why the UEFI Driver Model exists!

## Adapting Existing Drivers to `EFI_DT_IO_PROTOCOL`

Aside from figuring out device discovery (legacy vs. UEFI Driver Model) the
next big question to solve is how to interact with a device. The `GetReg()`
DT I/O Protocol function is similar to fetching the BAR info for a PCI
device. `GetReg()` will populate an `EFI_DT_REG` descriptor.

For some drivers, it will be easy enough to simply use appropriate DT
I/O Protocol functions (`ReadReg()` and friends), which operate
directly on the `EFI_DT_REG` descriptor.
[PciSioSerialDxe](../Drivers/PciSioSerialDxe/SerialIo.c#L1371) is a
good example.

Other drivers may be a bit more involved. Maybe you need the actual
CPU address. Maybe you'll need the untranslated bus address. Maybe
you'll need the length of the register
region. [VirtioFdtDxe](../Drivers/VirtioFdtDxe/DriverBinding.c#L441) is a
good example: it actually creates a child device for the managed DT
controller, to which a generic (Virtio10) driver binds. This generic
driver doesn't know anything about FDT or PCI controllers, so the
`VIRTIO_MMIO_DEVICE` needs the actual CPU address of the device. The
`TranslatedBase` field of a register descriptor is a CPU address
if the `BusDtIo` field is NULL, meaning that FdtBusDxe was able
to translate the bus address to a CPU address. If the field is not
NULL, then `TranslatedBase` is a bus address that is valid in the
context of the ancestor DT controller referenced by `BusDtIo`, and you
can only perform I/O using the DT I/O Protocol functions (`ReadReg()` and
friends, and [only if the ancestor device driver implements the I/O
callbacks](../Drivers/FdtBusDxe/DtIo.c#L438)).
