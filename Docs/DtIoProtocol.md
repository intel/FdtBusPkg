# EFI Devicetree I/O Protocol

> [!NOTE]
> See [FdtBusPkg Documentation Style and Terms Definitions](StyleAndTerms.md) first.

This section provides a detailed description of the `EFI_DT_IO_PROTOCOL`.
This protocol is used by code, typically platform device drivers,
running in the EFI boot services environment to access device I/O on a
DT controller. In particular, functions for managing platform devices
exposed via Devicetree as DT controllers are defined here.

The interfaces provided in the `EFI_DT_IO_PROTOCOL` are for performing
basic operations for device property access, register I/O, DMA buffer
handling and device enumeration. The system provides abstracted access
to basic system resources to allow a driver to have a programmatic
method to access these basic system resources. The main goal of this
protocol is to provide an abstraction that simplifies the writing of
device drivers for platform devices. This goal is accomplished by providing
the following features:
- A driver model that does not require the driver to hardcode or
search for platform devices to manage. Instead, drivers are provided the
location of the device to manage or have the capability to be notified
when a DT controller is discovered.
- A device driver model that abstracts device register access. A
driver does not have to manually parse _reg_ properties or traverse
parent Devicetree nodes to map back to a valid CPU address. Instead,
relative addressing is used for all register accesses, and the API
fully hides the complexity of performing such accesses. It even
supports register accesses via a parent DT controller device driver.
- The Device Path for the platform device can be obtained from the same
 device handle that the `EFI_DT_IO_PROTOCOL` resides on.
- A full set of functions to parse and return various device property values.
- Functions to perform bus mastering DMA. This includes both packet
based DMA and common buffer DMA, and deals with DMA range
restrictions, non-cache coherent DMA and CPU barriers where applicable.

## EFI_DT_IO_PROTOCOL

### Summary

Provides the basic device property, register I/O, DMA buffer and
device enumeration interfaces that a driver uses to access its DT
controller.

### GUID

```
#define EFI_DT_IO_PROTOCOL_GUID \
  { \
    0x5ce5a2b0, 0x2838, 0x3c35, {0x1e, 0xe3, 0x42, 0x5e, 0x36, 0x50, 0xa2, 0x9b } \
  }
```

### Protocol Interface Structure

```
typedef struct _EFI_DT_IO_PROTOCOL {
  //
  // Properties useful to most clients.
  //
  // Note: ComponentName is not CONST because the place where it
  // it mostly useful (EFI_COMPONENT_NAME_PROTOCOL) is missing
  // CONST qualifier in ComponentNameGetControllerName arg.
  //
  CHAR16                                 *ComponentName;
  CONST CHAR8                            *Name;
  CONST CHAR8                            *DeviceType;
  EFI_DT_STATUS                          DeviceStatus;
  UINT8                                  AddressCells;
  UINT8                                  SizeCells;
  UINT8                                  ChildAddressCells;
  UINT8                                  ChildSizeCells;
  BOOLEAN                                IsDmaCoherent;
  EFI_HANDLE                             ParentDevice;
  //
  // Core.
  //
  EFI_DT_IO_PROTOCOL_LOOKUP              Lookup;
  EFI_DT_IO_PROTOCOL_GET_PROP            GetProp;
  EFI_DT_IO_PROTOCOL_SCAN_CHILDREN       ScanChildren;
  EFI_DT_IO_PROTOCOL_REMOVE_CHILD        RemoveChild;
  EFI_DT_IO_PROTOCOL_SET_CALLBACKS       SetCallbacks;
  //
  // Convenience calls to use with or instead of GetProp.
  //
  EFI_DT_IO_PROTOCOL_PARSE_PROP          ParseProp;
  EFI_DT_IO_PROTOCOL_GET_STRING_INDEX    GetStringIndex;
  EFI_DT_IO_PROTOCOL_GET_U32             GetU32;
  EFI_DT_IO_PROTOCOL_GET_U64             GetU64;
  EFI_DT_IO_PROTOCOL_GET_U128            GetU128;
  EFI_DT_IO_PROTOCOL_GET_REG             GetReg;
  EFI_DT_IO_PROTOCOL_GET_REG_BY_NAME     GetRegByName;
  EFI_DT_IO_PROTOCOL_GET_RANGE           GetRange;
  EFI_DT_IO_PROTOCOL_GET_STRING          GetString;
  EFI_DT_IO_PROTOCOL_GET_DEVICE          GetDevice;
  EFI_DT_IO_PROTOCOL_IS_COMPATIBLE       IsCompatible;
  //
  // Device register access.
  //
  EFI_DT_IO_PROTOCOL_POLL_REG            PollReg;
  EFI_DT_IO_PROTOCOL_IO_REG              ReadReg;
  EFI_DT_IO_PROTOCOL_IO_REG              WriteReg;
  EFI_DT_IO_PROTOCOL_COPY_REG            CopyReg;
  //
  // DMA operations.
  //
  EFI_DT_IO_PROTOCOL_MAP                 Map;
  EFI_DT_IO_PROTOCOL_UNMAP               Unmap;
  EFI_DT_IO_PROTOCOL_ALLOCATE_BUFFER     AllocateBuffer;
  EFI_DT_IO_PROTOCOL_FREE_BUFFER         FreeBuffer;
} EFI_DT_IO_PROTOCOL;
```

### Members

| Name | Description |
| ---- | ----------- |
| `ComponentName` | DT device node name as a UTF-16 string. |
| `Name` | DT device node name. |
| `DeviceType` | _device_type_ property for the DT controller. |
| `DeviceStatus` | Device status reported as an `EFI_DT_STATUS`. |
| `AddressCells` | _#address-cells_ for this DT controller. |
| `SizeCells` | _#size-cells_ for this DT controller. |
| `ChildAddressCells` | _#address-cells_ for child DT controllers. |
| `ChildSizeCells` | _#size-cells_ for child DT controllers. |
| `IsDmaCoherent` | TRUE if DMA is cache coherent. |
| `ParentDevice` | `EFI_HANDLE` of the parent DT controller. |
| [`Lookup`](#efi_dt_io_protocollookup) | Looks up a DT controller handle by DT path or alias. |
| [`GetProp`](#efi_dt_io_protocolgetprop) | Looks up a property by name, populating an `EFI_DT_PROPERTY` iterator. |
| [`ScanChildren`](#efi_dt_io_protocolscanchildren) | Create device chandles for child DT devices. |
| [`RemoveChild`](#efi_dt_io_protocolremovechild) | Tears down a child DT controller created via `ScanChildren`. |
| [`SetCallbacks`](#efi_dt_io_protocolsetcallbacks) | Sets device driver callbacks to be used by the DT bus driver. |
| [`ParseProp`](#efi_dt_io_protocolparseprop) | Parses out a property field, advancing the `EFI_DT_PROPERTY` iterator. |
| [`GetStringIndex`](#efi_dt_io_protocolgetstringindex) | Looks up an index for a string in a string list property. |
| [`GetU32`](#efi_dt_io_protocolgetu32) | Looks up an `UINT32` property value by index. |
| [`GetU64`](#efi_dt_io_protocolgetu64) | Looks up an `UINT64` property value by index. |
| [`GetU128`](#efi_dt_io_protocolgetu128) | Looks up an `EFI_DT_U128` property value by index. |
| [`GetReg`](#efi_dt_io_protocolgetreg) | Looks up a _reg_ property value by index. |
| [`GetRegByName`](#efi_dt_io_protocolgetregbyname) | Looks up a _reg_ property value by name. |
| [`GetRange`](#efi_dt_io_protocolgetrange) | Looks up a ranges property value by index. |
| [`GetString`](#efi_dt_io_protocolgetstring) | Looks up a string property value by index. |
| [`GetDevice`](#efi_dt_io_protocolgetdevice) | Looks up a device `EFI_HANDLE` from a property value by index. |
| [`IsCompatible`](#efi_dt_io_protocoliscompatible) | Validates against the device _compatible_ property. |
| [`PollReg`](#efi_dt_io_protocolpollreg) | Polls a device register until an exit condition is met, or a timeout occurs. |
| [`ReadReg`](#efi_dt_io_protocolreadreg) | Reads a device register. |
| [`WriteReg`](#efi_dt_io_protocolwritereg) | Writes a device register. |
| [`CopyReg`](#efi_dt_io_protocolcopyreg) | Copies a region of device register space to another region of device register space. |
| [`Map`](#efi_dt_io_protocolmap) | Provides a DT controller-specific address needed to access system memory for DMA. |
| [`Unmap`](#efi_dt_io_protocolunmap) | Completes the `Map()` operation and releases any corresponding resources. |
| [`AllocateBuffer`](#efi_dt_io_protocolallocatebuffer) | Allocates pages that are suitable for a common buffer mapping. |
| [`FreeBuffer`](#efi_dt_io_protocolfreebuffer) | Frees memory allocated with `AllocateBuffer()`. |

### Related Definitions

```
typedef struct {
  VENDOR_DEVICE_PATH    VendorDevicePath;
  CHAR8                 Name[];
} EFI_DT_DEVICE_PATH_NODE;

typedef enum {
  EfiDtIoWidthUint8 = 0,
  EfiDtIoWidthUint16,
  EfiDtIoWidthUint32,
  EfiDtIoWidthUint64,
  EfiDtIoWidthFifoUint8,
  EfiDtIoWidthFifoUint16,
  EfiDtIoWidthFifoUint32,
  EfiDtIoWidthFifoUint64,
  EfiDtIoWidthFillUint8,
  EfiDtIoWidthFillUint16,
  EfiDtIoWidthFillUint32,
  EfiDtIoWidthFillUint64,
  EfiDtIoWidthMaximum
} EFI_DT_IO_PROTOCOL_WIDTH;

typedef enum {
  ///
  /// A read operation from system memory by a bus master.
  ///
  EfiDtIoDmaOperationBusMasterRead,
  ///
  /// A write operation from system memory by a bus master.
  ///
  EfiDtIoDmaOperationBusMasterWrite,
  ///
  /// Provides both read and write access to system memory
  /// by both the processor and a bus master. The buffer is
  /// coherent from both the processor's and the bus master's
  /// point of view.
  ///
  EfiDtIoDmaOperationBusMasterCommonBuffer,
  EfiDtIoDmaOperationMaximum
} EFI_DT_IO_PROTOCOL_DMA_OPERATION;

typedef unsigned __int128  EFI_DT_BUS_ADDRESS;
typedef unsigned __int128  EFI_DT_SIZE;
typedef UINT32             EFI_DT_CELL;
typedef unsigned __int128  EFI_DT_U128;

typedef struct {
  EFI_DT_BUS_ADDRESS    BusBase;
  EFI_DT_BUS_ADDRESS    TranslatedBase;
  EFI_DT_SIZE           Length;
  //
  // BusDevice == NULL means TranslatedBase is a
  // CPU address.
  //
  EFI_DT_IO_PROTOCOL    *BusDtIo;
} EFI_DT_REG;

typedef struct {
  EFI_DT_BUS_ADDRESS    ChildBase;
  EFI_DT_BUS_ADDRESS    ParentBase;
  EFI_DT_SIZE           Size;
} EFI_DT_RANGE;

typedef enum {
  EFI_DT_STATUS_BROKEN,
  EFI_DT_STATUS_OKAY,
  EFI_DT_STATUS_DISABLED,
  EFI_DT_STATUS_RESERVED,
  EFI_DT_STATUS_FAIL,
  EFI_DT_STATUS_FAIL_WITH_CONDITION,
} EFI_DT_STATUS;

///
/// Beginning, end of property data and pointer to data to be next returned.
///
typedef struct {
  ///
  /// Beginning of property data.
  ///
  CONST VOID    *Begin;
  ///
  /// Current pointer to data.
  ///
  CONST VOID    *Iter;
  ///
  /// End of property data.
  ///
  CONST VOID    *End;
} EFI_DT_PROPERTY;

typedef enum {
  ///
  /// A 32-bit value.
  ///
  EFI_DT_VALUE_U32,
  ///
  /// A 64-bit value.
  ///
  EFI_DT_VALUE_U64,
  ///
  /// A 128-bit value.
  ///
  EFI_DT_VALUE_U128,
  ///
  /// An address encoded by #address-cells.
  ///
  EFI_DT_VALUE_BUS_ADDRESS,
  ///
  /// An address encoded by #address-cells for a child node.
  ///
  EFI_DT_VALUE_CHILD_BUS_ADDRESS,
  ///
  /// A size encoded by #size-cells.
  ///
  EFI_DT_VALUE_SIZE,
  ///
  /// A size encoded by #size-cells for a child node.
  ///
  EFI_DT_VALUE_CHILD_SIZE,
  ///
  /// A reg property value.
  ///
  EFI_DT_VALUE_REG,
  ///
  /// A ranges/dma-ranges property value.
  ///
  EFI_DT_VALUE_RANGE,
  ///
  /// A string property value.
  ///
  EFI_DT_VALUE_STRING,
  ///
  /// An EFI_HANDLE for a device reference property.
  ///
  EFI_DT_VALUE_DEVICE
} EFI_DT_VALUE_TYPE;

///
/// EFI_DT_IO_PROTOCOL_CB allows a device driver to provide some
/// callbacks for use by the bus driver.
///
typedef struct _EFI_DT_IO_PROTOCOL_CB {
  EFI_DT_IO_PROTOCOL_IO_REG    ReadChildReg;
  EFI_DT_IO_PROTOCOL_IO_REG    WriteChildReg;
} EFI_DT_IO_PROTOCOL_CB;
```

### Description

The `EFI_DT_IO_PROTOCOL` provides the basic device property, register
I/O, DMA buffer and device enumeration interfaces that are used to
abstract accesses to DT controllers.There is one `EFI_DT_IO_PROTOCOL`
instance for each  supported device node in a Devicetree. A device
driver that wishes to manage a DT controller in a system will have to
retrieve the `EFI_DT_IO_PROTOCOL` instance that is associated with the
DT controller. A device handle for a DT controller will minimally
contain an `EFI_DEVICE_PATH_PROTOCOL` instance and an
`EFI_DT_IO_PROTOCOL` instance.

### Property Parsing

Devicetree properties can encode integers, bus addresses, sizes,
strings, even references to other nodes. The properties values can be
structured in a complex manner as arrays or by mixing different kinds
of value types. To make parsing simpler, the API is centered around
sequential parsing, treating the property value as a stream of data
that can be methodically parsed via subsequent calls.

Consider this sample Devicetree snippet:
```
parent@0 {
  #address-cells = <2>;
  #size-cells = <2>;
  ...
  child@0 {
    ...
    reg = <0x1 0x00000002 0x3 0x00000004>,
          <0x5 0x00000006 0x7 0x00000008>,
          <0x9 0x0000000A 0xB 0x0000000C>,
          <0xD 0x0000000E 0xF 0x00000011>,
          <0x12 0x00000013 0x14 0x00000015>;
    reg-names = "apple", "banana", "orange", "grape", "peach";
  };
};
```

Properties are looked up via `GetProp()`, which returns an
`EFI_DT_PROPERTY` iterator to be passed into subsequent `ParseProp()`
calls, which take the value type parsed and inex as additional
parameters. A successful `ParseProp()` call updates the iterator. E.g.:

```
CONST CHAR8     *String;
EFI_DT_PROPERTY Property;
...
ASSERT (ChildDtIo->GetProp (ChildDtIo, "reg-names", &Property) ==
EFI_SUCCESS);
//
// Return the first string value pointed to, i.e. "apple".
//
ASSERT (ChildDtIo->ParseProp (ChildDtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_SUCCESS);
ASSERT (AsciiStrCmp (String, "apple") == 0);
//
//
// Return the first string value pointed to, i.e. "banana", because
// the previous ParseProp updated the EFI_DT_PROPERTY iterator.
//
ASSERT (ChildDtIo->ParseProp (ChildDtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_SUCCESS);
ASSERT (AsciiStrCmp (String, "banana") == 0);
//
// Return the second string value pointed to, i.e. skipping "peach".
//
ASSERT (ChildDtIo->ParseProp (ChildDtIo, &Property, EFI_DT_VALUE_STRING, 1, &String) == EFI_SUCCESS);
ASSERT (AsciiStrCmp (String, "grape") == 0);
```

This may seem like a chore for handling the default case, like
looking up a register range, a simple integer and so on, which is why
many convenience wrappers have been added. These just wrap `GetProp()`
and `ParseProp()`. E.g.:

```
CONST CHAR8 *String;

ASSERT (ChildDtIo->GetString (ChildDtIo, "reg-names", 2, &String) == EFI_SUCCESS);
ASSERT (AsciiStrCmp (String, "peach") == 0);
```

Any driver will need to check compatiblity with a DT controller by
querying the _compatible_ property string list and comparing against
known-good identifiers. This is greatly simplified via the
`IsCompatible()` convenience wrapper:

```
ASSERT (DtIo->IsCompatible (DtIo, "pci-host-ecam-generic") == EFI_SUCCESS);
```

A common pattern seen in Devicetree is associating string names with
array indexes. In the Devicetree snippet above, a _reg-names_ property
is a string list with as many strings as there are values in the _reg_
property array. So code will just look up a register range that
matches the index of _banana_. This allows the Devicetree to be more
self-descriptive, and provides a degree of resilience against future
schema/binding changes.

Use the `GetStringIndex()` convenience wrapper:

```
EFI_DT_REG Reg;
UINTN      Index;

ASSERT (ChildDtIo->GetStringIndex (ChildDtIo, "reg-names", "banana", &Index) == EFI_SUCCESS);
ASSERT (Index == 1);
ASSERT (ChildDtIo->GetReg (ChildDtIo, Index, &Reg) == EFI_SUCCESS);
```

The `GetRegByName()` function is a convenience wrapper specifically
for _reg_ and _reg-names_, for even less typing:

```
EFI_DT_REG Reg;

ASSERT (ChildDtIo->GetRegByName (ChildDtIo, "banana", &Reg) == EFI_SUCCESS);
```

> [!NOTE]
> Looking up an `EFI_DT_REG` does a bit more than parsing out
> an `EFI_DT_BUS_ADDRESS` and an `EFI_DT_SIZE`. `GetReg()` and related
> calls perform translation of bus addresses to CPU addresses.

### Register Access

The facilities provided mirror those available in
`EFI_PCI_IO_PROTOCOL`: `PollReg()`, `ReadReg()`, `WriteReg()` and
`CopyReg()`. These operate on the `EFI_DT_REG` values returned by
`GetReg())` and related calls. `EFI_DT_IO_PROTOCOL_WIDTH` exactly
follows the `EFI_CPU_IO_PROTOCOL_WIDTH` definitions, and allows for
advanced operations, such as filling a range with the same value,
or writing out a buffer into a single register. Ultimately
the accesses are performed via `EFI_CPU_IO2_PROTOCOL`.

It's possible there is no direct translation between bus
and CPU addresses. For example, PHY register accesses might involve
a custom mechanism only known to a NIC driver. Unless the parent DT
controller device driver set child register read and write callbacks
via `SetCallbacks()`, calls to read, write, poll and copy registers
will fail with `EFI_UNSUPPORTED`.

### DMA

Bus mastering DT controllers can use the DMA services for DMA
operations. There are three basic types of bus mastering DMA that is
supported by this protocol. These are DMA reads by a bus master, DMA
writes by a bus master, and common buffer DMA. The DMA read and write
operations may need to be broken into smaller chunks. The caller of
Map() must pay attention to the number of bytes that were mapped, and
if required, loop until the entire buffer has been transferred. This
section lists the different bus mastering DMA operations that
are supported, and the sequence of `EFI_DT_IO_PROTOCOL` interfaces that
are used for each DMA operation type.

#### DMA Bus Master Read Operation

- Fill buffer with data for the DMA Bus Master to read.
- Call `Map()` with `EfiDtIoDmaOperationBusMasterRead`.
- Program the DMA Bus Master with the `*DeviceAddress` returned by `Map()`.
- Start the DMA.
- Wait for DMA to complete.
- Call `Unmap()`.

> [!WARNING]
> Don't make assumptions on the behavior of `Map()` and expect that
> any further CPU changes to buffer made after the `Map()` or
> `Unmap()` calls are visible to the device. `Map()` could be performing
> bounce buffering, cache flushes, CPU barriers/fences, IOMMU/SMMU
> configuration, etc.

#### DMA Bus Master Write Operation

- Call `Map()` with `EfiDtIoDmaOperationBusMasterWrite`
- Program the DMA Bus Master with the `*DeviceAddress` returned by `Map()`.
- Start the DMA.
- Wait for DMA to complete.
- Call `Unmap()`.
- Read data written by the DMA Bus Master.

> [!WARNING]
> Don't make assumptions on the behavior of `Map()` and `Unmap()`, and
> expect the CPU to see any modified data prior to the `Unmap()` call
> or expect any further device changes to buffer after the `Unmap()`
> to be visible. There could be bounce buffering involved, cache
> flushes, CPU barriers/fences, IOMMU/SMMU configuration, etc.

#### DMA Bus Master Common Buffer Operation

Provides access to a memory region coherent from both the processor's
and the bus master's point of view. How this is accomplished depends
on the capabilities of the device, as divined by the DT bus driver
(e.g. via _dma-coherent_, _dma-ranges_).

- Call `AllocateBuffer()` to allocate the common buffer.
- Call `Map()` with `EfiDtIoDmaOperationBusMasterCommonBuffer`.
- Program the DMA Bus Master with the `*DeviceAddress` returned by `Map()`.
- The common buffer can now be accessed equally by the processor and the DMA bus master.
- Call `Unmap()`.
- Call `FreeBuffer()`.

> [!NOTE]
> Common buffer operations provide some guarantees, e.g. not
> involving bounce buffering or cache flushes. Generally
> common buffer DMA operations are easier to retrofit into existing
> driver code that assumes cache-coherent DMA with equivalent CPU
> and bus addresses.

> [!TIP]
> In some situations (e.g. limited shared memory requiring bounce buffering)
> `AllocateBuffer()` may fail. It is highly encouraged to rewrite legacy
> code to avoid common buffer operations.

> [!CAUTION]
> Using common buffer operations doesn't absolve the code from performing
> CPU barrier operations if required by the CPU architecture, that
> are otherwise done by `Map()` and `Unmap()`. See https://github.com/intel/FdtBusPkg/issues/19 for work on adding
> these.

### `EFI_DT_IO_PROTOCOL.Lookup()`
#### Description
  Looks up an EFI_DT_IO_PROTOCOL handle given a DT path or alias,
optionally connecting any missing drivers along the way.

`PathOrAlias` could be an:
- alias (i.e. via DT /aliases)
- relative DT path (foo/bar), relative to the device described by `This`.
- absolute DT path (/foo/bar).

> [!CAUTION]
> The unit address portion of the DT path may not be omitted under any circumstances. Passing "/soc/pci@30000000" is okay but "/soc/pci" is not. This is a restriction in the current implementation and a difference in behavior, when compared to the [Devicetree Specification, Section 2.2.3](https://devicetree-specification.readthedocs.io/en/stable/devicetree-basics.html#path-names).

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_LOOKUP)(
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *PathOrAlias,
  IN  BOOLEAN             Connect,
  OUT EFI_HANDLE          *FoundHandle
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `PathOrAlias` | DT path or alias looked up. |
| `Connect` | Connect missing drivers during lookup. |
| `FoundHandle` | Matching `EFI_HANDLE`. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Not found. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetProp()`
#### Description

Looks up a property by name. Returns the `EFI_DT_PROPERTY` iterator
that can be subsequently passed to `ParseProp()` calls.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_GET_PROP)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  OUT EFI_DT_PROPERTY    *Property
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Name` | Property to look up. |
| `Property` | Pointer to the `EFI_DT_PROPERTY` to fill. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Could not find property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.ScanChildren()`
#### Description

Create child handles with EFI_DT_IO_PROTOCOL for children nodes.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_SCAN_CHILDREN)(
  IN  EFI_DT_IO_PROTOCOL       *This,
  IN  EFI_HANDLE                DriverBindingHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `DriverBindingHandle` | Driver binding handle. |
| `RemainingDevicePath` | If present, describes the child handle that needs to be created |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Child handles created (all or 1 if `RemainingDevicePath` was not `NULL`) |
| `EFI_NOT_FOUND` | No child handles created. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.RemoveChild()`
#### Description

Tears down a child DT controller created via `ScanChildren`.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_REMOVE_CHILD)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  EFI_HANDLE          ChildHandle,
  IN  EFI_HANDLE          DriverBindingHandle
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `ChildHandle` | Child handle to tear down. |
| `DriverBindingHandle` | Driver binding handle. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Child handle destroyed. |
| `EFI_UNSUPPORTED` | Child handle doesn't support `EFI_DT_IO_PROTOCOL`. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.SetCallbacks()`
#### Description

Sets device driver callbacks to be used by the DT bus driver.

It is the responsibility of the device driver to set `NULL` callbacks
when stopping on a handle, as the bus driver cannot detect when a
driver disconnects. The function signature here thus both encourages
appropriate use and helps detect bugs. The bus driver will validate
`AgentHandle` and `Callbacks`. The operation will fail if `AgentHandle`
doen't match the current driver managing the handle. The operation
will also fail when trying to set callbacks when these are already set.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_SET_CALLBACKS)(
  IN  EFI_DT_IO_PROTOCOL           *This,
  IN  EFI_HANDLE                   AgentHandle,
  IN  EFI_DT_IO_PROTOCOL_CB        *Callbacks
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `AgentHandle` | Driver managing the DT controller referenced by `This`.|
| `Callbacks` | Pointer to structure with callback functions. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Success. |
| `EFI_INVALID_PARAMETER` | Invalid parameter. |
| `EFI_ACCESS_DENIED` | AgentHandle/Callbacks validation failed. |

### `EFI_DT_IO_PROTOCOL.ParseProp()`
#### Description

Parses out a property field, advancing the `EFI_DT_PROPERTY` iterator.

> [!CAUTION]
> Parsing `EFI_DT_VALUE_U128` and `EFI_DT_VALUE_DEVICE` types is not implemented today.

#### Prototype

```
typedef EFI_STATUS (EFIAPI *EFI_DT_IO_PROTOCOL_PARSE_PROP)(
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  OUT EFI_DT_PROPERTY *Prop,
  IN  EFI_DT_VALUE_TYPE   Type,
  IN  UINTN               Index,
  OUT VOID                *Buffer
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Prop` | `EFI_DT_PROPERTY` describing the property buffer and current position.|
| `Type` | Type of the field to parse out.|
| `Index` | Index of the field to return, starting from the current buffer position within the `EFI_DT_PROPERTY`.|
| `Buffer` | Pointer to a buffer large enough to contain the parsed out field.|

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Parsing successful. |
| `EFI_NOT_FOUND` | Not enough remaining property buffer to contain the field of specified type. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetStringIndex()`
#### Description

Looks up an index for a string in a string list property.

This is useful to look up other properties indexed by name,
e.g. consider:
```
foo = <value1>, <value2>, <value3>;
foo-names = "index1", "index2", "index3";
```

#### Prototype

```
typedef EFI_STATUS (EFIAPI *EFI_DT_IO_PROTOCOL_GET_STRING_INDEX)(
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  CONST CHAR8         *Value,
  OUT UINTN               *Index
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Name` | Property to examine. |
| `Value` | String to search for. |
| `Index` | Pointer for returning found index. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | String found. |
| `EFI_NOT_FOUND` | Could not find property or string. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetU32()`
#### Description

Looks up a `UINT32` property value by index.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_GET_U32)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT UINT32             *U32
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Name` | Name of the property.|
| `Index` | Index of the value to return. |
| `U32` | Where to return the value. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Could not find property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetU64()`
#### Description

Looks up a `UINT64` property value by index.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_GET_U64)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT UINT64             *U64
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Name` | Name of the property.|
| `Index` | Index of the value to return. |
| `U64` | Where to return the value. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Could not find property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetU128()`
#### Description

Looks up an `EFI_DT_U128` property value by index.

> [!CAUTION]
> Not implemented at the moment.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_GET_U128)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT EFI_DT_U128        *U128
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Name` | Name of the property.|
| `Index` | Index of the value to return. |
| `U128` | Where to return the value. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Could not find property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetReg()`
#### Description

Looks up a _reg_ property value by index, returning an
`EFI_DT_REG`. The latter can be passed to the [register access API](#register-access).

> [!NOTE]
> The returned address is in CPU space, not bus space,
> if these are different. That is, `GetReg()` performs
> automatic address translation, and does not return
> the raw values encoded in the Devicetree property.

> [!CAUTION]
> Only support for direct translation is implemented today (CPU == bus addresses).

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_GET_REG)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  UINTN              Index,
  OUT EFI_DT_REG         *Reg
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Index` | Index of the _reg_ value to return. |
| `Reg` | Pointer to the `EFI_DT_REG` to fill. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Could not find property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetRegByName()`
#### Description

Looks up a _reg_ property value by name, returning an
`EFI_DT_REG`. The latter can be passed to the [register access API](#register-access).

> [!NOTE]
> Lookup by name involves examining the _reg-names_ property.

> [!NOTE]
> The returned address is in CPU space, not bus space,
> if these are different. That is, `GetRegByName()` performs
> automatic address translation, and does not return
> the raw values encoded in the Devicetree property.

> [!CAUTION]
> Only support for direct translation is implemented today (CPU == bus addresses).

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_GET_REG_BY_NAME)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CHAR8              *Name,
  OUT EFI_DT_REG         *Reg
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Name` | Name of the _reg_ value to return. |
| `Reg` | Pointer to the `EFI_DT_REG` to fill. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Could not find property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetRange()`
#### Description

Looks up a ranges property value by index.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_GET_RANGE)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CHAR8              *Name,
  IN  UINTN              Index,
  OUT EFI_DT_RANGE       *Range
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Name` | Name of the ranges property to examine. |
| `Index` | Index of the ranges value to return. |
| `Range` | Pointer to an `EFI_DT_RANGE`. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Could not find property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetString()`
#### Description

Looks up a string property value by index.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_GET_STRING)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT CONST CHAR8        **String
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Name` | Name of the property. |
| `Index` | Index of the string to return. |
| `String` | Pointer to a CHAR8* to fill. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Could not find property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.GetDevice()`
#### Description

Looks up a DT controller `EFI_HANDLE` from property value by
index.

> [!CAUTION]
> Not implemented at the moment.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_GET_DEVICE)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT EFI_HANDLE         *Handle
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Name` | Name of the property. |
| `Index` | INdex of the device to return. |
| `Handle` | Pointer to an `EFI_HANDLE` to fill. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Lookup successful. |
| `EFI_NOT_FOUND` | Could not find property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.IsCompatible()`

#### Description

Validates a string against the device _compatible_ property.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_IS_COMPATIBLE)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *CompatibleString
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `CompatibleString` | String to validate. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | CompatibleString is present in the _compatible_ property. |
| `EFI_NOT_FOUND` | String is not present in the _compatible_ property. |
| `EFI_DEVICE_ERROR` | Devicetree error. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.PollReg()`
#### Description

Polls a device register until an exit condition is met, or a timeout occurs.

This function provides a standard way to poll a DT controller register
location. The result of this register read operation is stored in
`Result`. The read operation is repeated until either a timeout of
`Delay` 100 ns units has expired, or `(Result & Mask)` is equal to
`Value`.

This function will always perform at least one register access no
matter how small `Delay` may be. If `Delay` is 0, then `Result` will
be returned with a status of `EFI_SUCCESS` even if `Result` does not
match the exit criteria. If `Delay` expires, then `EFI_TIMEOUT` is
returned.

If `Width` is not `EfiDtIoWidthUint8`, `EfiDtIoWidthUint16`,
`EfiDtIoWidthUint32` or `EfiDtIoWidthUint64`, then
`EFI_INVALID_PARAMETER` is returned.

The I/O operations are carried out exactly as requested. The caller is
responsible for any alignment and I/O width issues which the bus,
device, platform, or type of I/O might require.

All register accesses generated by this function are guaranteed to be
observable before this function returns.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_POLL_REG)(
  IN  EFI_DT_IO_PROTOCOL           *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH     Width,
  IN  EFI_DT_REG                   *Reg,
  IN  EFI_DT_SIZE                  Offset,
  IN  UINT64                       Mask,
  IN  UINT64                       Value,
  IN  UINT64                       Delay,
  OUT UINT64                       *Result
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Width `| Encodes the width of the I/O operation. |
| `Reg` | Pointer to a register space descriptor. |
| `Offset` | The offset within the selected register space to start the I/O operation. |
| `Mask` | Mask used for the polling criteria. Bytes above `Width` in `Mask` are ignored. The bits in the bytes below `Width` which are zero in `Mask` are ignored when polling the register. |
| `Value` | The comparison value used for the polling exit criteria. |
| `Delay` | The number of 100 ns units to poll. |
| `Result` | Pointer to the last value read from the I/O location. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | The last data returned from the access matched the poll exit criteria. |
| `EFI_UNSUPPORTED` | Offset is not valid for the register space specified. |
| `EFI_TIMEOUT` | Delay expired before a match occurred. |
| `EFI_OUT_OF_RESOURCES` | The request could not be completed due to a lack of resources. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.ReadReg()`

See [`WriteReg()`](#efi_dt_io_protocolwritereg).

### `EFI_DT_IO_PROTOCOL.WriteReg()`
#### Description

Reads or writes a device register.

The I/O operations are carried out exactly as requested. The caller is
responsible for any alignment and I/O width issues which the bus,
device, platform, or type of I/O might require.

If `Width` is `EfiDtIoWidthUint8`, `EfiDtIoWidthUint16`,
`EfiDtIoWidthUint32` or `EfiDtIoWidthUint64`, then both the register
address and `Buffer` are incremented for each of the `Count`
operations performed.

If `Width` is `EfiDtIoWidthFifoUint8`, `EfiDtIoWidthFifoUint16`,
`EfiDtIoWidthFifoUint32` or `EfiDtIoWidthFifoUint64`, then only
`Buffer` is incremented for each of the `Count` operations
performed. The read or write operation is performed `Count` times on
the same register address.

If `Width` is `EfiDtIoWidthFillUint8`, `EfiDtIoWidthFillUint16`,
`EfiDtIoWidthFillUint32` or `EfiDtIoWidthFillUint64`, then only the
register address is incremented for each of the `Count` operations
performed. The read or write operation is performed `Count` times from
the first element of `Buffer`.

All register accesses generated by this function are guaranteed to be
observable before this function returns.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_IO_REG)(
  IN     EFI_DT_IO_PROTOCOL          *This,
  IN     EFI_DT_IO_PROTOCOL_WIDTH    Width,
  IN     EFI_DT_REG                  *Reg,
  IN     EFI_DT_SIZE                 Offset,
  IN     UINTN                       Count,
  IN OUT VOID                        *Buffer
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Width` | Encodes the width and stride of the I/O operation. |
| `Reg` | Pointer to a register space descriptor. |
| `Offset` | The offset within the selected register space to start the I/O operation. |
| `Count` | The number of I/O operations to perform. |
| `Buffer` | For reads, the destination buffer to store the results. For writes, the source buffer. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | The data was read from or written to the device. |
| `EFI_UNSUPPORTED` | The address range specified by `Offset`, `Width` and `Count` is not valid for the register space specified by `Reg`.
| `EFI_OUT_OF_RESOURCES` | The request could not be completed due to a lack of resources. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |

### `EFI_DT_IO_PROTOCOL.CopyReg()`
#### Description

Copies a region of device register space to another region of device
register space.

The I/O operations are carried out exactly as requested. The caller is
responsible for any alignment and I/O width issues which the bus,
device, platform, or type of I/O might require.

If `Width` is `EfiDtIoWidthUint8`, `EfiDtIoWidthUint16`,
`EfiDtIoWidthUint32` or `EfiDtIoWidthUint64`, then `Count` read/write
transactions are performed to move the contents of the `SrcOffset`
region to the `DestOffset` region. The implementation must be reentrant,
and it must handle overlapping `SrcOffset` and `DestOffset` regions if
`SrcReg` and `DestReg` are the same.

The contents of the `DestOffset` region on exit from this service must
match the contents of the `SrcOffset` region on entry to this
service. Due to potential overlaps, the contents of the `SrcOffset`
region may be modified by this service.

All register accesses generated by this function are guaranteed to be
observable before this function returns.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_COPY_REG)(
  IN  EFI_DT_IO_PROTOCOL          *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH    Width,
  IN  EFI_DT_REG                  *DestReg,
  IN  EFI_DT_SIZE                 DestOffset,
  IN  EFI_DT_REG                  *SrcReg,
  IN  EFI_DT_SIZE                 SrcOffset,
  IN  UINTN                       Count
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Width` | Encodes the width and stride of the I/O operation. |
| `DestReg` | Destination register space descriptor. |
| `DestOffset` | Offset within `DestReg` for I/O writes. |
| `SrcReg` | Source register space descripor. |
| `SrcOffset` | Offset within `SrcReg` for I/O reads. |
| `Count` | Number of I/O operations to perform. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | The data was copied from one I/O region to another. |
| `EFI_UNSUPPORTED` | The address range specified by `DestOffset`, `Width` and `Count` is not valid for `DestReg`, or the address range specified by `SrcOffset`, `Width` and `Count` is not valid for `SrcReg`. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |
| `EFI_OUT_OF_RESOURCES` | The request could not be completed due to a lack of resources. |

### `EFI_DT_IO_PROTOCOL.Map()`
#### Description

Provides a DT controller-specific address needed to access system
memory for DMA. This function is used to map system memory for DT
controller DMA accesses.

> [!CAUTION]
> Not implemented at the moment.

All DT controller bus master accesses must be performed through their
mapped addresses and such mappings must be freed with
`Unmap()` when complete. If the bus master access is
a single read or write data transfer, then
`EfiDtIoDmaOperationBusMasterRead` or `EfiDtIoDmaOperationBusMasterWrite` is
used and the range is unmapped to complete the operation. If
performing an `EfiDtIoDmaOperationBusMasterRead`, all the data
must be present in system memory before the `Map()` is
performed. Similarly, if performing an
`EfiDtIoDmaOperationBusMasterWrite`, the data cannot be properly accessed
in system memory until `Unmap()` is performed.

Bus master operations that require both read and write access or
require multiple host device interactions within the same mapped
region must use `EfiDtIoDmaOperationBusMasterCommonBuffer`. However, only
memory allocated via the `AllocateBuffer()` interface can be mapped
for this operation type.

In all mapping requests the resulting `NumberOfBytes` actually mapped
may be less than the requested amount. In this case, the DMA operation
will have to be broken up into smaller chunks. The `Map()` function will
map as much of the DMA operation as it can at one time. The caller may
have to loop on `Map()` and `Unmap()` in order to complete a large DMA
transfer.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_MAP)(
  IN      EFI_DT_IO_PROTOCOL                *This,
  IN      EFI_DT_IO_PROTOCOL_DMA_OPERATION  Operation,
  IN      VOID                              *HostAddress,
  IN  OUT UINTN                             *NumberOfBytes,
  OUT     EFI_PHYSICAL_ADDRESS              *DeviceAddress,
  OUT     VOID                              **Mapping
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Operation` | Indicates if the bus master is going to read or write to system memory. |
| `HostAddress` | The system memory address to map to the device. |
| `NumberOfBytes` | On input the number of bytes to map. On output the number of bytes that were mapped.|
| `DeviceAddress` | The resulting map address for the bus master device to use to access the host's HostAddress.|
| `Mapping` | A resulting value to pass to `Unmap()`.|

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | The range was mapped for the returned `NumberOfBytes`. |
| `EFI_UNSUPPORTED` | The `HostAddress` cannot be mapped as a common buffer. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |
| `EFI_OUT_OF_RESOURCES` | The request could not be completed due to a lack of resources. |
| `EFI_DEVICE_ERROR` | The system hardware could not map the requested address. |

### `EFI_DT_IO_PROTOCOL.Unmap()`
#### Description

Completes the `Map()` operation and releases any corresponding
resources. If the operation was an
`EfiDtIoDmaOperationBusMasterWrite`, the data is committed to the
target system memory.

> [!CAUTION]
> Not implemented at the moment.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_UNMAP)(
  IN  EFI_DT_IO_PROTOCOL          *This,
  IN  VOID                        *Mapping
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Mapping` | The mapping value returned from `Map()`. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | The range was unmapped. |
| `EFI_DEVICE_ERROR` | The data was not committed to the target system memory. |

### `EFI_DT_IO_PROTOCOL.AllocateBuffer()`
#### Description

Allocates pages that are suitable for a common buffer mapping.

> [!CAUTION]
> Not implemented at the moment.

The `AllocateBuffer()` function allocates pages that are suitable for an
`EfiDtIoDmaOperationBusMasterCommonBuffer` mapping. This means that the
buffer allocated by this function must support simultaneous access by
both the processor and the DT controller. The device address that the
DT controller uses to access the buffer can be retrieved with a call
to `Map()`.

If the memory allocation specified by `MemoryType` and `Pages` cannot be
satisfied, then `EFI_OUT_OF_RESOURCES` is returned.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_ALLOCATE_BUFFER)(
  IN  EFI_DT_IO_PROTOCOL           *This,
  IN  EFI_MEMORY_TYPE              MemoryType,
  IN  UINTN                        Pages,
  OUT VOID                         **HostAddress
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `MemoryType` | The type of memory to allocate, `EfiBootServicesData` or `EfiRuntimeServicesData.` |
| `Pages` | The number of pages to allocate. |
| `HostAddress` | A pointer to store the base system memory address of the allocated range. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | The requested memory pages were allocated. The requested memory pages were allocated. |
| `EFI_INVALID_PARAMETER` | One or more parameters are invalid. |
| `EFI_OUT_OF_RESOURCES` | The memory pages could not be allocated. |

### `EFI_DT_IO_PROTOCOL.FreeBuffer()`
#### Description

Frees memory allocated with `AllocateBuffer()`.

> [!CAUTION]
> Not implemented at the moment.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_FREE_BUFFER)(
  IN  EFI_DT_IO_PROTOCOL           *This,
  IN  UINTN                        Pages,
  IN  VOID                         *HostAddress
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` | A pointer to the `EFI_DT_IO_PROTOCOL` instance. |
| `Pages` | The number of pages to free. |
| `HostAddress` | The base system memory address of the allocated range. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | The requested memory pages were freed. |
| `EFI_INVALID_PARAMETER` | The memory range specified by `HostAddress` and `Pages` was not allocated with `AllocateBuffer()`. |
