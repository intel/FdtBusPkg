# EFI Device Tree I/O Protocol

> [!NOTE]
> See [FdtBusPkg Documentation Style and Terms Definitions](StyleAndTerms.md) first.

This section provides a detailed description of the `EFI_DT_IO_PROTOCOL`.
This protocol is used by code, typically platform device drivers,
running in the EFI boot services environment to access device I/O on a
DT controller. In particular, functions for managing platform devices
exposed via Device Tree as DT controllers are defined here.

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
parent Device Tree nodes to map back to a valid CPU address. Instead,
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
| `Lookup` | Looks up a DT controller handle by DT path or alias. |
| `GetProp` | Looks up a property by name, populating an `EFI_DT_PROPERTY` iterator. |
| `ScanChildren` | Create device chandles for child DT devices. |
| `RemoveChild` | Tears down a child DT controller created via `ScanChildren`. |
| `SetCallbacks` | Sets device driver callbacks to be used by the DT bus driver. |
| `ParseProp` | Parses out a property field, advancing the `EFI_DT_PROPERTY` iterator. |
| `GetStringIndex` | Looks up an index for a string in a string list property. |
| `GetU32` | Looks up an `EFI_DT_U32` property value by index. |
| `GetU64` | Looks up an `EFI_DT_U64` property value by index. |
| `GetU128` | Looks up an `EFI_DT_U128` property value by index. |
| `GetReg` | Looks up a _reg_ property by index. |
| `GetRegByName` | Looks up a _reg_ property by name. |
| `GetRange` | Looks up a _ranges_ property value by index. |
| `GetString` | Looks up a string property value by index. |
| `GetDevice` | Looks up a device `EFI_HANDLE` from a property valie by index. |
| `IsCompatible` | Validates against the device _compatible_ property |
| `PollReg` | Polls a device register until an exit condition is met, or a timeout occurs. |
| `ReadReg` | Reads a device register. |
| `WriteReg` | Writes a device register. |
| `CopyReg` | Copies a region of device register space to another region of device register space. |
| `Map` | Provides a DT controller-specific address needed to access system memory for DMA. |
| `Unmap` | Completes the `Map()` operation and releases any corresponding resources. |
| `AllocateBuffer` | Allocates pages that are suitable for a common buffer mapping. |
| `FreeBuffer` | Frees memory allocated with `AllocateBuffer()`. |

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
typedef UINT32    EFI_DT_CELL;
typedef unsigned __int128  EFI_DT_U128;

typedef struct {
  EFI_DT_BUS_ADDRESS    Base;
  EFI_DT_SIZE           Length;
  //
  // BusDevice == NULL means Base is a CPU real address.
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

The `EFI_DT_IO_PROTOCOL` provides the basic device properrty, register
I/O, DMA buffer and device enumeration interfaces that are used to
abstract accesses to DT controllers.There is one `EFI_DT_IO_PROTOCOL`
instance for each  supported device node in a Device Tree. A device
driver that wishes to manage a DT controller in a system will have to
retrieve the `EFI_DT_IO_PROTOCOL` instance that is associated with the
DT controller. A device handle for a DT controller will minimally
contain an `EFI_DEVICE_PATH_PROTOCOL` instance and an
`EFI_DT_IO_PROTOCOL` instance.

### Property parsing

### DMA