/** @file
  EFI Device Tree I/O Protocol provides the basic Property, Register and
  DMA interfaces that a driver uses to access a device exposed using a
  Device Tree node.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DT_IO_H__
#define __DT_IO_H__

#include <Protocol/DevicePath.h>

#define EFI_DT_IO_PROTOCOL_GUID \
  { \
    0x5ce5a2b0, 0x2838, 0x3c35, {0x1e, 0xe3, 0x42, 0x5e, 0x36, 0x50, 0xa2, 0x9b } \
  }

#define EFI_DT_NODE_DATA_PROTOCOL_GUID \
  { \
    0x5ce5a2b0, 0x2838, 0x3c35, {0x1e, 0xe3, 0x42, 0x5e, 0x36, 0xcc, 0xbb, 0xaa } \
  }

typedef struct {
  VENDOR_DEVICE_PATH    VendorDevicePath;
  CHAR8                 Name[];
} EFI_DT_DEVICE_PATH_NODE;

typedef struct _EFI_DT_NODE_DATA_PROTOCOL  EFI_DT_NODE_DATA_PROTOCOL;
typedef struct _EFI_DT_IO_PROTOCOL         EFI_DT_IO_PROTOCOL;

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
  //// point of view.
  ///
  EfiDtIoDmaOperationBusMasterCommonBuffer,
  EfiDtIoDmaOperationMaximum
} EFI_DT_IO_PROTOCOL_DMA_OPERATION;

typedef UINTN  EFI_DT_ADDRESS;
typedef UINTN  EFI_DT_SIZE;

typedef struct {
  EFI_DT_ADDRESS    Base;
  EFI_DT_SIZE       Length;
} EFI_DT_REG;

typedef struct {
  EFI_DT_ADDRESS    ChildBase;
  EFI_DT_ADDRESS    ParentBase;
  EFI_DT_SIZE       Size;
} EFI_DT_RANGE;

typedef enum {
  EFI_DT_STATUS_OKAY,
  EFI_DT_STATUS_DISABLED,
  EFI_DT_STATUS_RESERVED,
  EFI_DT_STATUS_FAIL,
  EFI_DT_STATUS_FAIL_WITH_CONDITION,
} EFI_DT_STATUS;

//
// Beginning, end of property data and pointer to data to be next returned.
//
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
  /// An address encoded by #address-cells.
  ///
  EFI_DT_VALUE_ADDRESS,
  ///
  /// A size encoded by #size-cells.
  ///
  EFI_DT_VALUE_SIZE,
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
  /// A reference to another EFI_DT_IO_PROTOCOL.
  ///
  EFI_DT_VALUE_LOOKUP
} EFI_DT_VALUE_TYPE;

/**
  Looks up an EFI_DT_IO_PROTOCOL instance given a path or alias.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  PathOrAlias           Path or alias looked up.
  @param  Device                Pointer to the EFI_DT_IO_PROTOCOL located.

  @retval EFI_SUCCESS           Lookup successful
  @retval EFI_NOT_FOUND         Could not resolve PathOrAlias to a EFI_DT_IO_PROTOCOL
                                instance.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_LOOKUP)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *PathOrAlias,
  OUT EFI_DT_IO_PROTOCOL **Device
  );

/**
  Looks up property by name for a EFI_DT_IO_PROTOCOL instance.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Property to look up.
  @param  Property              Pointer to the EFI_DT_PROPERTY to fill.

  @retval EFI_SUCCESS           Lookup successful
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_PROP)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  OUT EFI_DT_PROPERTY    *Property
  );

/**
  For a Device Tree node associated with the EFI_DT_IO_PROTOCOL instance,
  create child handles with EFI_DT_IO_PROTOCOL for children nodes.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  RemainingDevicePath   If present, describes the child handle that
                                needs to be created.

  @retval EFI_SUCCESS           Child handles created (all or 1 if RemainingDevicePath
                                was not NULL)
  @retval EFI_NOT_FOUND         No child handles created
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_SCAN_CHILDREN)(
  IN  EFI_DT_IO_PROTOCOL       *This,
  IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
  );

/**
  For a Device Tree node associated with the EFI_DT_IO_PROTOCOL instance,
  tear down child handles with EFI_DT_IO_PROTOCOL on them. If NumberOfChildren
  is not 0, only tear down the handles specified in ChildHandleBuffer.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  NumberOfChildren      The number of child device handles in ChildHandleBuffer.
  @param  ChildHandleBuffer     An array of child handles to be freed. May be NULL if
                                NumberOfChildren is 0.

  @retval EFI_SUCCESS           Child handles created (all or 1 if RemainingDevicePath
                                was not NULL)
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_REMOVE_CHILDREN)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  UINTN              NumberOfChildren,
  IN  EFI_HANDLE         *ChildHandleBuffer OPTIONAL
  );

/**
  Looks up a reg property value by name for a EFI_DT_IO_PROTOCOL instance.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Property to look up.
  @param  Index                 Index of the reg value to return.
  @param  Reg                   Pointer to the EFI_DT_REG to fill.

  @retval EFI_SUCCESS           Lookup successful
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_REG)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT EFI_DT_REG         *Reg
  );

/**
  Validates CompatibleString against the compatible property array for a
  EFI_DT_IO_PROTOCOL instance.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  CompatibleString      String to compare against the compatible property
                                array.

  @retval EFI_SUCCESS           CompatibleString is present in the compatible
                                property array.
  @retval EFI_NOT_FOUND         CompatibleString is notpresent in the compatible
                                property array.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_IS_COMPATIBLE)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *CompatibleString
  );

/**
  Parses out a field encoded in the property, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Type                  Type of the field to parse out.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  Buffer                Pointer to a buffer large enough to contain the
                                parsed out field.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef EFI_STATUS (EFIAPI *EFI_DT_IO_PROTOCOL_PARSE_PROP)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  EFI_DT_PROPERTY    *Prop,
  IN  EFI_DT_VALUE_TYPE  Type,
  IN  UINTN              Index,
  OUT VOID               *Buffer
  );

/**
  Reads from the register space of a device. Returns either when the polling exit criteria is
  satisfied or after a defined duration.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Width                 Signifies the width of the I/O operations.
  @param  Reg                   Pointer to a register space descriptor.
  @param  Offset                The offset within the selected register space to start the
                                I/O operation.
  @param  Mask                  Mask used for the polling criteria.
  @param  Value                 The comparison value used for the polling exit criteria.
  @param  Delay                 The number of 100 ns units to poll.
  @param  Result                Pointer to the last value read from the I/O location.

  @retval EFI_SUCCESS           The last data returned from the access matched the poll exit criteria.
  @retval EFI_UNSUPPORTED       Offset is not valid for the register space specified by Reg.
  @retval EFI_TIMEOUT           Delay expired before a match occurred.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_POLL_REG)(
  IN  EFI_DT_IO_PROTOCOL           *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH     Width,
  IN  EFI_DT_REG                   *Reg,
  IN  UINT64                       Offset,
  IN  UINT64                       Mask,
  IN  UINT64                       Value,
  IN  UINT64                       Delay,
  OUT UINT64                       *Result
  );

/**
  Enable a driver to access device registers.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Width                 Signifies the width of the I/O operations.
  @param  Reg                   Pointer to a register space descriptor.
  @param  Offset                The offset within the selected register space to start the
                                I/O operation.
  @param  Count                 The number of I/O operations to perform.
  @param  Buffer                For read operations, the destination buffer to store the results. For write
                                operations, the source buffer to write data from.

  @retval EFI_SUCCESS           The data was read from or written to the device.
  @retval EFI_UNSUPPORTED       The address range specified by Offset, Width, and Count is not
                                valid for the register space specified by Reg.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_IO_REG)(
  IN     EFI_DT_IO_PROTOCOL          *This,
  IN     EFI_DT_IO_PROTOCOL_WIDTH    Width,
  IN     EFI_DT_REG                  *Reg,
  IN     UINT64                      Offset,
  IN     UINTN                       Count,
  IN OUT VOID                        *Buffer
  );

typedef struct {
  ///
  /// Read device registers.
  ///
  EFI_DT_IO_PROTOCOL_IO_REG    Read;
  ///
  /// Writ device registers.
  ///
  EFI_DT_IO_PROTOCOL_IO_REG    Write;
} EFI_DT_IO_PROTOCOL_ACCESS;

/**
  Enables a driver to copy one region of device register space to another region of device
  register space.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Width                 Signifies the width of the memory operations.
  @param  DestReg               Pointer to register space descriptor to use as the base address
                                for the memory operation to perform.
  @param  DestOffset            The destination offset within the register space specified by DestReg
                                to start the memory writes for the copy operation.
  @param  SrcReg                Pointer to register space descriptor to use as the base address
                                for the memory operation to perform.
  @param  SrcOffset             The source offset within the register space specified by SrcReg
                                to start the memory reads for the copy operation.
  @param  Count                 The number of memory operations to perform. Bytes moved is Width
                                size * Count, starting at DestOffset and SrcOffset.

  @retval EFI_SUCCESS           The data was copied from one memory region to another memory region.
  @retval EFI_UNSUPPORTED       The address range specified by DestOffset, Width, and Count
                                is not valid for the register space specified by DestReg.
  @retval EFI_UNSUPPORTED       The address range specified by SrcOffset, Width, and Count is
                                is not valid for the register space specified by SrcReg.
  @retval EFI_INVALID_PARAMETER Width is invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_COPY_REG)(
  IN  EFI_DT_IO_PROTOCOL          *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH    Width,
  IN  EFI_DT_REG                  *DestReg,
  IN  UINT64                      DestOffset,
  IN  EFI_DT_REG                  *SrcReg,
  IN  UINT64                      SrcOffset,
  IN  UINTN                       Count
  );

/**
  Provides the device-specific addresses needed to access system memory.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Operation             Indicates if the bus master is going to read or write to system memory.
  @param  HostAddress           The system memory address to map to the device.
  @param  NumberOfBytes         On input the number of bytes to map. On output the number of bytes
                                that were mapped.
  @param  DeviceAddress         The resulting map address for the bus master device to use to access
                                the hosts HostAddress.
  @param  Mapping               A resulting value to pass to Unmap().

  @retval EFI_SUCCESS           The range was mapped for the returned NumberOfBytes.
  @retval EFI_UNSUPPORTED       The HostAddress cannot be mapped as a common buffer.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_DEVICE_ERROR      The system hardware could not map the requested address.

**/
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

/**
  Completes the Map() operation and releases any corresponding resources.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Mapping               The mapping value returned from Map().

  @retval EFI_SUCCESS           The range was unmapped.
  @retval EFI_DEVICE_ERROR      The data was not committed to the target system memory.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_UNMAP)(
  IN  EFI_DT_IO_PROTOCOL          *This,
  IN  VOID                        *Mapping
  );

/**
  Allocates pages that are suitable for an EfiDtIoDmaOperationBusMasterCommonBuffer
  mapping.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  MemoryType            The type of memory to allocate, EfiBootServicesData or
                                EfiRuntimeServicesData.
  @param  Pages                 The number of pages to allocate.
  @param  HostAddress           A pointer to store the base system memory address of the
                                allocated range.

  @retval EFI_SUCCESS           The requested memory pages were allocated.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The memory pages could not be allocated.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_ALLOCATE_BUFFER)(
  IN  EFI_DT_IO_PROTOCOL           *This,
  IN  EFI_MEMORY_TYPE              MemoryType,
  IN  UINTN                        Pages,
  OUT VOID                         **HostAddress
  );

/**
  Frees memory that was allocated with AllocateBuffer().

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Pages                 The number of pages to free.
  @param  HostAddress           The base system memory address of the allocated range.

  @retval EFI_SUCCESS           The requested memory pages were freed.
  @retval EFI_INVALID_PARAMETER The memory range specified by HostAddress and Pages
                                was not allocated with AllocateBuffer().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_FREE_BUFFER)(
  IN  EFI_DT_IO_PROTOCOL           *This,
  IN  UINTN                        Pages,
  IN  VOID                         *HostAddress
  );

///
/// EFI_DT_NODE_DATA_PROTOCOL is primarily used internally by FdtBusDxe, but can
/// be also used by clients wishing to have low-level access via libfdt.
///
struct _EFI_DT_NODE_DATA_PROTOCOL {
  CHAR8    *Name;
  INTN     FdtNode;
};

///
/// The EFI_DT_IO_PROTOCOL provides the basic Property, Register and DMA
/// interfaces used to abstract access to devices exposed using a Device
/// Tree node.Memory, I/O, PCI configuration,
///
/// There is one EFI_DT_IO_PROTOCOL instance for each device node in a
/// Device Tree.
///
/// A device driver that wishes to manage a device described by a Device
/// Tree node will have to retrieve the EFI_DT_IO_PROTOCOL instance that
/// is associated with the device.
///
struct _EFI_DT_IO_PROTOCOL {
  CHAR8                                 *Name;
  CHAR8                                 *Model;
  EFI_DT_STATUS                         DeviceStatus;
  UINT8                                 AddressCells;
  UINT8                                 SizeCells;
  BOOLEAN                               IsDmaCoherent;
  ///
  /// Core.
  ///
  EFI_DT_IO_PROTOCOL_LOOKUP             Lookup;
  EFI_DT_IO_PROTOCOL_GET_PROP           GetProp;
  EFI_DT_IO_PROTOCOL_SCAN_CHILDREN      ScanChildren;
  EFI_DT_IO_PROTOCOL_REMOVE_CHILDREN    RemoveChildren;
  ///
  /// Convenience calls to use with or instead of GetProp.
  ///
  EFI_DT_IO_PROTOCOL_PARSE_PROP         ParseProp;
  EFI_DT_IO_PROTOCOL_GET_REG            GetReg;
  EFI_DT_IO_PROTOCOL_IS_COMPATIBLE      IsCompatible;

  ///
  /// Device register access.
  ///
  EFI_DT_IO_PROTOCOL_POLL_REG           PollReg;
  EFI_DT_IO_PROTOCOL_IO_REG             ReadReg;
  EFI_DT_IO_PROTOCOL_IO_REG             WriteReg;
  EFI_DT_IO_PROTOCOL_COPY_REG           CopyReg;
  ///
  /// DMA operations.
  ///
  EFI_DT_IO_PROTOCOL_MAP                Map;
  EFI_DT_IO_PROTOCOL_UNMAP              Unmap;
  EFI_DT_IO_PROTOCOL_ALLOCATE_BUFFER    AllocateBuffer;
  EFI_DT_IO_PROTOCOL_FREE_BUFFER        FreeBuffer;
};

extern EFI_GUID  gEfiDtNodeDataProtocol;
extern EFI_GUID  gEfiDtIoProtocolGuid;

#endif /* __DT_IO_H__ */
