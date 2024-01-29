/** @file
  EFI Devicetree I/O Protocol provides the basic Property, Register and
  DMA interfaces that a driver uses to access a device exposed using a
  Devicetree node.

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

typedef struct {
  VENDOR_DEVICE_PATH    VendorDevicePath;
  CHAR8                 Name[];
} EFI_DT_DEVICE_PATH_NODE;

typedef struct _EFI_DT_IO_PROTOCOL     EFI_DT_IO_PROTOCOL;
typedef struct _EFI_DT_IO_PROTOCOL_CB  EFI_DT_IO_PROTOCOL_CB;

//
// This matches the CpuIo2 EFI_CPU_IO_PROTOCOL_WIDTH.
// When we go 128-bit, this will need work.
//

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

///
/// Convert EFI_DT_IO_PROTOCOL_WIDTH to the actual width.
///
#define DT_IO_PROTOCOL_WIDTH(x)  (1 << (x & 0x3))

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

/**
  Looks up an EFI_DT_IO_PROTOCOL handle given a DT path or alias, optionally
  connecting any missing drivers along the way.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  PathOrAlias           DT path or alias looked up.
  @param  Connect               Connect missing drivers during lookup.
  @param  FoundHandle           Matching EFI_HANDLE.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Not found.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_LOOKUP)(
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *PathOrAlias,
  IN  BOOLEAN             Connect,
  OUT EFI_HANDLE          *FoundHandle
  );

/**
  Looks up property by name.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Property to look up.
  @param  Property              Pointer to the EFI_DT_PROPERTY to fill.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_PROP)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  OUT EFI_DT_PROPERTY    *Property
  );

/**
  For a Devicetree node associated with the EFI_DT_IO_PROTOCOL instance,
  create child handles with EFI_DT_IO_PROTOCOL for children nodes.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  DriverBindingHandle   Driver binding handle.
  @param  RemainingDevicePath   If present, describes the child handle that
                                needs to be created.

  @retval EFI_SUCCESS           Child handles created (all or 1 if RemainingDevicePath
                                was not NULL).
  @retval EFI_NOT_FOUND         No child handles created.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_SCAN_CHILDREN)(
  IN  EFI_DT_IO_PROTOCOL       *This,
  IN  EFI_HANDLE                DriverBindingHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL
  );

/**
  For a Devicetree node associated with the EFI_DT_IO_PROTOCOL instance,
  tear down the specified child handle.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  ChildHandle           Child handle to tear down.
  @param  DriverBindingHandle   Driver binding handle.

  @retval EFI_SUCCESS           Child handle destroyed.
  @retval EFI_UNSUPPORTED       Child handle doesn't support EFI_DT_IO_PROTOCOL.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_REMOVE_CHILD)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  EFI_HANDLE          ChildHandle,
  IN  EFI_HANDLE          DriverBindingHandle
  );

/**
  Looks up a U32 property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the reg value to return.
  @param  U32                   Pointer to a UINT32.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_U32)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT UINT32             *U32
  );

/**
  Looks up a U64 property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the reg value to return.
  @param  U64                   Pointer to a UINT64.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_U64)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT UINT64             *U64
  );

/**
  Looks up an EFI_DT_U128 property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the reg value to return.
  @param  U128                  Pointer to an EFI_DT_U128.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_U128)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT EFI_DT_U128        *U128
  );

/**
  Looks up a reg property value by index.

  Note: The returned address is in CPU space, not bus space, if these are
  different.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Index                 Index of the reg value to return.
  @param  Reg                   Pointer to the EFI_DT_REG to fill.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_REG)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  UINTN              Index,
  OUT EFI_DT_REG         *Reg
  );

/**
  Looks up a reg property value by name.

  Note: Lookups by name involve examining the reg-names property.

  Note 2: The returned address is in CPU space, not bus space, if these are
  different.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the reg value to return.
  @param  Reg                   Pointer to the EFI_DT_REG to fill.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_REG_BY_NAME)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CHAR8              *Name,
  OUT EFI_DT_REG         *Reg
  );

/**
  Looks up a ranges property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the ranges property to examine.
  @param  Index                 Index of the ranges value to return.
  @param  Range                 Pointer to an EFI_DT_RANGE.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_RANGE)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CHAR8              *Name,
  IN  UINTN              Index,
  OUT EFI_DT_RANGE       *Range
  );

/**
  Looks up a string property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the reg value to return.
  @param  String                Pointer to a CHAR8*.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_STRING)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT CONST CHAR8 **String
  );

/**
  Looks up a device EFI_HANDLE from property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the reg value to return.
  @param  Handle                Pointer to an EFI_HANDLE.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS(EFIAPI *EFI_DT_IO_PROTOCOL_GET_DEVICE)(
  IN  EFI_DT_IO_PROTOCOL *This,
  IN  CONST CHAR8        *Name,
  IN  UINTN              Index,
  OUT EFI_HANDLE         *Handle
  );

/**
  Validates CompatibleString against the compatible property array for a
  EFI_DT_IO_PROTOCOL instance.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  CompatibleString      String to compare against the compatible property
                                array.

  @retval EFI_SUCCESS           CompatibleString is present in the compatible
                                property array.
  @retval EFI_NOT_FOUND         CompatibleString is not present in the compatible
                                property array.
  @retval EFI_DEVICE_ERROR      Devicetree error.
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
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  OUT EFI_DT_PROPERTY *Prop,
  IN  EFI_DT_VALUE_TYPE   Type,
  IN  UINTN               Index,
  OUT VOID                *Buffer
  );

/**
  Given a string list property name and a value of one of the strings,
  returns the strings index.

  This is useful to look up other properties indexed by name, e.g.
  foo = < value1 value2 value3 >
  foo-names = "index1", "index2", "index3"

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Property to examine.
  @param  Value                 String to search for.

  @retval EFI_SUCCESS           String found.
  @retval EFI_NOT_FOUND         Could not find property or string.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef EFI_STATUS (EFIAPI *EFI_DT_IO_PROTOCOL_GET_STRING_INDEX)(
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  CONST CHAR8         *Value,
  OUT UINTN               *Index
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
  IN  EFI_DT_SIZE                  Offset,
  IN  UINT64                       Mask,
  IN  UINT64                       Value,
  IN  UINT64                       Delay,
  OUT UINT64                       *Result
  );

/**
  Enable a driver to access device registers (reading or writing).

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
  IN     EFI_DT_SIZE                 Offset,
  IN     UINTN                       Count,
  IN OUT VOID                        *Buffer
  );

/**
  Enables a driver to copy one region of device register space to another region of device
  register space.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Width                 Signifies the width of I/O operations.
  @param  DestReg               Pointer to register space descriptor to use as the base address
                                for the I/O operation to perform.
  @param  DestOffset            The destination offset within the register space specified by DestReg
                                to start the I/O writes for the copy operation.
  @param  SrcReg                Pointer to register space descriptor to use as the base address
                                for the I/O operation to perform.
  @param  SrcOffset             The source offset within the register space specified by SrcReg
                                to start the I/O reads for the copy operation.
  @param  Count                 The number of I/O operations to perform. Bytes moved is Width
                                size * Count, starting at DestOffset and SrcOffset.

  @retval EFI_SUCCESS           The data was copied from one I/O region to another I/O region.
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
  IN  EFI_DT_SIZE                 DestOffset,
  IN  EFI_DT_REG                  *SrcReg,
  IN  EFI_DT_SIZE                 SrcOffset,
  IN  UINTN                       Count
  );

/**
  Provides the device-specific addresses needed to access system memory.

  Note: the implementation of this may perform cache coherency operations,
  bounce buffering, etc, as necessary.

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

  Note: the implementation of this may perform cache coherency operations,
  bounce buffering, etc, as necessary.

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

/**
  Sets device driver callbacks to be used by the bus driver.

  It is the responsibility of the device driver to set NULL callbacks
  when stopping on a handle, as the bus driver cannot detect when a driver
  disconnects. The function signature here thus both encourages appropriate
  use and helps detect bugs. The bus driver will validate AgentHandle
  and Callbacks. The operation will fail if AgentHandle doen't match the
  current driver managing the handle. The operation will also fail when
  trying to set callbacks when these are already set.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  AgentHandle           EFI_HANDLE.
  @param  Callbacks             EFI_DT_IO_PROTOCOL_CB.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER Invalid parameter.
  @retval EFI_ACCESS_DENIED     AgentHandle/Callbacks validation.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_IO_PROTOCOL_SET_CALLBACKS)(
  IN  EFI_DT_IO_PROTOCOL           *This,
  IN  EFI_HANDLE                   AgentHandle,
  IN  EFI_DT_IO_PROTOCOL_CB        *Callbacks
  );

///
/// EFI_DT_IO_PROTOCOL_CB allows a device driver to provide some
/// callbacks for use by the bus driver.
///
struct _EFI_DT_IO_PROTOCOL_CB {
  EFI_DT_IO_PROTOCOL_IO_REG    ReadChildReg;
  EFI_DT_IO_PROTOCOL_IO_REG    WriteChildReg;
};

///
/// The EFI_DT_IO_PROTOCOL provides the basic Property, Register and DMA
/// interfaces used to abstract access to devices exposed using a Device
/// Tree node.
///
/// There is one EFI_DT_IO_PROTOCOL instance for each supported device
/// node in a Devicetree.
///
/// A device driver that wishes to manage a device described by a Device
/// Tree node will have to retrieve the EFI_DT_IO_PROTOCOL instance that
/// is associated with the device.
///
struct _EFI_DT_IO_PROTOCOL {
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
};

extern EFI_GUID  gEfiDtIoProtocolGuid;

#endif /* __DT_IO_H__ */
