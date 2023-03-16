/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

**/

#include "FdtBusDxe.h"

/**
  Looks up an EFI_DT_IO_PROTOCOL instance given a path or alias.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  PathOrAlias           Path or alias looked up.
  @param  Device                Pointer to the EFI_DT_IO_PROTOCOL located.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not resolve PathOrAlias to a EFI_DT_IO_PROTOCOL
                                instance.
  @retval EFI_DEVICE_ERROR      Device Tree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoLookup (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *PathOrAlias,
  OUT EFI_DT_IO_PROTOCOL  **Device
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Looks up property by name for a EFI_DT_IO_PROTOCOL instance.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Property to look up.
  @param  Property              Pointer to the EFI_DT_PROPERTY to fill.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Device Tree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetProp (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  OUT EFI_DT_PROPERTY     *Property
  )
{
  return EFI_UNSUPPORTED;
}

/**
  For a Device Tree node associated with the EFI_DT_IO_PROTOCOL instance,
  create child handles with EFI_DT_IO_PROTOCOL for children nodes.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  RemainingDevicePath   If present, describes the child handle that
                                needs to be created.

  @retval EFI_SUCCESS           Child handles created (all or 1 if RemainingDevicePath
                                was not NULL).
  @retval EFI_NOT_FOUND         No child handles created.
  @retval EFI_DEVICE_ERROR      Device Tree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoScanChildren (
  IN  EFI_DT_IO_PROTOCOL        *This,
  IN  EFI_DEVICE_PATH_PROTOCOL  *RemainingDevicePath OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

/**
  For a Device Tree node associated with the EFI_DT_IO_PROTOCOL instance,
  tear down child handles with EFI_DT_IO_PROTOCOL on them. If NumberOfChildren
  is not 0, only tear down the handles specified in ChildHandleBuffer.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  NumberOfChildren      The number of child device handles in ChildHandleBuffer.
  @param  ChildHandleBuffer     An array of child handles to be freed. May be NULL if
                                NumberOfChildren is 0.

  @retval EFI_SUCCESS           Child handles created (all or 1 if RemainingDevicePath
                                was not NULL).
  @retval EFI_DEVICE_ERROR      Device Tree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoRemoveChildren (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               NumberOfChildren,
  IN  EFI_HANDLE          *ChildHandleBuffer OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Looks up a reg property value by name for a EFI_DT_IO_PROTOCOL instance.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Index                 Index of the reg value to return.
  @param  Reg                   Pointer to the EFI_DT_REG to fill.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Device Tree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetReg (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               Index,
  OUT EFI_DT_REG          *Reg
  )
{
  UINTN         ElemLen;
  UINT8         AddressCells;
  UINT8         SizeCells;
  UINT8         Iter;
  int           Len;
  CONST UINT32  *Buf;
  DT_DEVICE     *DtDevice;

  DtDevice     = DT_DEV_FROM_THIS (This);
  AddressCells = This->AddressCells;
  SizeCells    = This->SizeCells;
  ElemLen      = AddressCells + SizeCells;

  Buf = fdt_getprop (gDeviceTreeBase, DtDevice->FdtNode, "reg", &Len);
  if (Buf == NULL) {
    if (Len == -FDT_ERR_NOTFOUND) {
      return EFI_NOT_FOUND;
    }

    return EFI_DEVICE_ERROR;
  }

  if ((Len / ElemLen) <= Index) {
    return EFI_NOT_FOUND;
  }

  Buf += (ElemLen * Index) / sizeof (UINT32);

  Reg->Base = 0;
  for (Iter = 0; Iter < AddressCells; Iter++, Buf++) {
    Reg->Base |= ((UINTN)fdt32_to_cpu (*Buf)) <<
                 (32 * (AddressCells - (Iter + 1)));
  }

  Reg->Length = 0;
  for (Iter = 0; Iter < SizeCells; Iter++, Buf++) {
    Reg->Length |= ((UINTN)fdt32_to_cpu (*Buf)) <<
                   (32 * (SizeCells - (Iter + 1)));
  }

  //
  // TODO: need to translate address instead of assuming
  // reg always encodes a CPU address.
  //

  return EFI_SUCCESS;
}

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
  @retval EFI_DEVICE_ERROR      Device Tree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoIsCompatible (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *CompatibleString
  )
{
  INTN       Ret;
  DT_DEVICE  *DtDevice;

  DtDevice = DT_DEV_FROM_THIS (This);

  Ret = fdt_node_check_compatible (
          gDeviceTreeBase,
          DtDevice->FdtNode,
          CompatibleString
          );
  if (Ret == 0) {
    return EFI_SUCCESS;
  } else if (Ret == 1) {
    return EFI_NOT_FOUND;
  }

  return EFI_DEVICE_ERROR;
}

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
EFI_STATUS
EFIAPI
DtIoParseProp (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_DT_PROPERTY     *Prop,
  IN  EFI_DT_VALUE_TYPE   Type,
  IN  UINTN               Index,
  OUT VOID                *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

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
EFI_STATUS
EFIAPI
DtIoPollReg (
  IN  EFI_DT_IO_PROTOCOL        *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN  EFI_DT_REG                *Reg,
  IN  UINT64                    Offset,
  IN  UINT64                    Mask,
  IN  UINT64                    Value,
  IN  UINT64                    Delay,
  OUT UINT64                    *Result
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Enable a driver to write registers.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Width                 Signifies the width of the I/O operations.
  @param  Reg                   Pointer to a register space descriptor.
  @param  Offset                The offset within the selected register space to start the
                                I/O operation.
  @param  Count                 The number of I/O operations to perform.
  @param  Buffer                The source buffer to write data from.

  @retval EFI_SUCCESS           The data was written to the device.
  @retval EFI_UNSUPPORTED       The address range specified by Offset, Width, and Count is not
                                valid for the register space specified by Reg.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoWriteReg (
  IN     EFI_DT_IO_PROTOCOL        *This,
  IN     EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN     EFI_DT_REG                *Reg,
  IN     UINT64                    Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Enable a driver to read device registers.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Width                 Signifies the width of the I/O operations.
  @param  Reg                   Pointer to a register space descriptor.
  @param  Offset                The offset within the selected register space to start the
                                I/O operation.
  @param  Count                 The number of I/O operations to perform.
  @param  Buffer                The destination buffer to store the results.

  @retval EFI_SUCCESS           The data was read from the device.
  @retval EFI_UNSUPPORTED       The address range specified by Offset, Width, and Count is not
                                valid for the register space specified by Reg.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoReadReg (
  IN     EFI_DT_IO_PROTOCOL        *This,
  IN     EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN     EFI_DT_REG                *Reg,
  IN     UINT64                    Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

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
EFI_STATUS
EFIAPI
DtIoCopyReg (
  IN  EFI_DT_IO_PROTOCOL        *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN  EFI_DT_REG                *DestReg,
  IN  UINT64                    DestOffset,
  IN  EFI_DT_REG                *SrcReg,
  IN  UINT64                    SrcOffset,
  IN  UINTN                     Count
  )
{
  return EFI_UNSUPPORTED;
}

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
EFI_STATUS
EFIAPI
DtIoMap (
  IN      EFI_DT_IO_PROTOCOL                *This,
  IN      EFI_DT_IO_PROTOCOL_DMA_OPERATION  Operation,
  IN      VOID                              *HostAddress,
  IN  OUT UINTN                             *NumberOfBytes,
  OUT     EFI_PHYSICAL_ADDRESS              *DeviceAddress,
  OUT     VOID                              **Mapping
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Completes the Map() operation and releases any corresponding resources.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Mapping               The mapping value returned from Map().

  @retval EFI_SUCCESS           The range was unmapped.
  @retval EFI_DEVICE_ERROR      The data was not committed to the target system memory.

**/
EFI_STATUS
EFIAPI
DtIoUnmap (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  VOID                *Mapping
  )
{
  return EFI_UNSUPPORTED;
}

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
EFI_STATUS
EFIAPI
DtIoAllocateBuffer (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_MEMORY_TYPE     MemoryType,
  IN  UINTN               Pages,
  OUT VOID                **HostAddress
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Frees memory that was allocated with AllocateBuffer().

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Pages                 The number of pages to free.
  @param  HostAddress           The base system memory address of the allocated range.

  @retval EFI_SUCCESS           The requested memory pages were freed.
  @retval EFI_INVALID_PARAMETER The memory range specified by HostAddress and Pages
                                was not allocated with AllocateBuffer().

**/
EFI_STATUS
EFIAPI
DtIoFreeBuffer (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               Pages,
  IN  VOID                *HostAddress
  )
{
  return EFI_UNSUPPORTED;
}
