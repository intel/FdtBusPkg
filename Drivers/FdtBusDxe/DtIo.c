/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

//
// For convenience, EFI_DT_IO_PROTOCOL_WIDTH exactly follows the CpuIo2
// definitions.
//
#define _(x)  (UINTN)(EfiDtIoWidth##x) == (UINTN)(EfiCpuIoWidth##x), #x
STATIC_ASSERT (_ (Uint8));
STATIC_ASSERT (_ (Uint16));
STATIC_ASSERT (_ (Uint32));
STATIC_ASSERT (_ (Uint64));
STATIC_ASSERT (_ (FifoUint8));
STATIC_ASSERT (_ (FifoUint16));
STATIC_ASSERT (_ (FifoUint32));
STATIC_ASSERT (_ (FifoUint64));
STATIC_ASSERT (_ (FillUint8));
STATIC_ASSERT (_ (FillUint16));
STATIC_ASSERT (_ (FillUint32));
STATIC_ASSERT (_ (FillUint64));
STATIC_ASSERT (_ (Maximum));
#undef _

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
EFI_STATUS
EFIAPI
DtIoLookup (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *PathOrAlias,
  IN  BOOLEAN             Connect,
  OUT EFI_HANDLE          *FoundHandle
  )
{
  DT_DEVICE                 *DtDevice;
  VOID                      *TreeBase;
  CONST CHAR8               *Resolved;
  CHAR8                     *Copied;
  CHAR8                     *Iter;
  EFI_DEVICE_PATH_PROTOCOL  *CurrentDp;
  EFI_STATUS                Status;

  if ((This == NULL) || (PathOrAlias == NULL) || (FoundHandle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CurrentDp = NULL;
  DtDevice  = DT_DEV_FROM_THIS (This);
  TreeBase  = GetTreeBaseFromDeviceFlags (DtDevice->Flags);

  //
  // PathOrAlias could be an:
  // - alias
  // - relative path (foo/bar), relative to DtDevice.
  // - absolute path (/foo/bar)
  //
  //
  Resolved = fdt_get_alias (TreeBase, PathOrAlias);
  if (Resolved == NULL) {
    //
    // Not an alias.
    //
    Resolved = PathOrAlias;
  }

  //
  // Copy as we're going to mutate it as part of parsing.
  //
  Copied = Iter = AllocateCopyPool (AsciiStrSize (Resolved), Resolved);

  if (*Iter == '/') {
    Iter++;
    CurrentDp = AppendDevicePath ((VOID *)GetDtRootFromDeviceFlags (DtDevice->Flags)->DevicePath, NULL);
  } else {
    CurrentDp = AppendDevicePath ((VOID *)DtDevice->DevicePath, NULL);
  }

  if (CurrentDp == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: CurrentDp\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Out;
  }

  while (*Iter != '\0') {
    EFI_DEVICE_PATH_PROTOCOL  *NewDp;
    EFI_DT_DEVICE_PATH_NODE   *DpNode;
    CHAR8                     *StartOfName;
    CHAR8                     *EndOfName;

    StartOfName = Iter;
    EndOfName   = AsciiStrChr (Iter, '/');
    if (EndOfName != NULL) {
      *EndOfName = '\0';
      Iter       = EndOfName + 1;
    } else {
      Iter = Iter + AsciiStrLen (Iter);
    }

    DpNode = FbpPathNodeCreate (StartOfName);
    if (DpNode == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: DtDevicePathNodeCreate\n", __func__));
      Status = EFI_OUT_OF_RESOURCES;
      goto Out;
    }

    NewDp = (VOID *)AppendDevicePathNode (CurrentDp, (VOID *)DpNode);
    FreePool (DpNode);
    if (NewDp == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: AppendDevicePathNode\n", __func__));
      Status = EFI_OUT_OF_RESOURCES;
      goto Out;
    }

    FreePool (CurrentDp);
    CurrentDp = NewDp;
  }

  //
  // At this point we have a fully assembled device path to
  // the handle of interest. Using DtPathToHandle/LocateDevicePath
  // imposes a restrictions on the lookup. Namely, the unit
  // address portion of the node name may not be omitted. Supporting
  // fuzzy matching will require an entirely different O(N^2)
  // lookup mechanism (or tracking children in DtDevice nodes).
  //
  Status = DtPathToHandle (CurrentDp, Connect, FoundHandle);

Out:
  if (CurrentDp != NULL) {
    FreePool (CurrentDp);
  }

  FreePool (Copied);
  return Status;
}

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
EFI_STATUS
EFIAPI
DtIoScanChildren (
  IN  EFI_DT_IO_PROTOCOL        *This,
  IN  EFI_HANDLE                DriverBindingHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL  *RemainingDevicePath OPTIONAL
  )
{
  DT_DEVICE  *DtDevice;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  return DtDeviceScan (
           DtDevice,
           (EFI_DT_DEVICE_PATH_NODE *)RemainingDevicePath,
           DriverBindingHandle
           );
}

/**
  For a Devicetree node associated with the EFI_DT_IO_PROTOCOL instance,
  tear down the specified child handle.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  ChildHandle           Child handle to tear down.
  @param  DriverBindingHandle   Driver binding handle.

  @retval EFI_SUCCESS           Child handle destroyed.
  @retval EFI_UNSUPPORTED       Child handle doesn't support EFI_DT_IO_PROTOCOL.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoRemoveChild (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_HANDLE          ChildHandle,
  IN  EFI_HANDLE          DriverBindingHandle
  )
{
  DT_DEVICE  *DtDevice;

  DtDevice = DT_DEV_FROM_THIS (This);
  return DtDeviceRemove (ChildHandle, DtDevice->Handle, DriverBindingHandle);
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
  @retval EFI_DEVICE_ERROR      Devicetree error.
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
  VOID       *TreeBase;

  if ((This == NULL) || (CompatibleString == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);
  TreeBase = GetTreeBaseFromDeviceFlags (DtDevice->Flags);

  Ret = fdt_node_check_compatible (
          TreeBase,
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
  IN  EFI_DT_SIZE               Offset,
  IN  UINT64                    Mask,
  IN  UINT64                    Value,
  IN  UINT64                    Delay,
  OUT UINT64                    *Result
  )
{
  EFI_STATUS  Status;
  UINT64      NumberOfTicks;
  UINT32      Remainder;
  UINT64      StartTick;
  UINT64      EndTick;
  UINT64      CurrentTick;
  UINT64      ElapsedTick;
  UINT64      Frequency;

  if ((This == NULL) || (Result == NULL) || (Width >= EfiDtIoWidthMaximum)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // No matter what, always do a single poll.
  //
  Status = DtIoReadReg (This, Width, Reg, Offset, 1, Result);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Delay == 0) {
    return EFI_SUCCESS;
  } else {
    //
    // NumberOfTicks = Frenquency * Delay / EFI_TIMER_PERIOD_SECONDS(1)
    //
    Frequency     = GetPerformanceCounterProperties (&StartTick, &EndTick);
    NumberOfTicks = MultThenDivU64x64x32 (Frequency, Delay, (UINT32)EFI_TIMER_PERIOD_SECONDS (1), &Remainder);
    if (Remainder >= (UINTN)EFI_TIMER_PERIOD_SECONDS (1) / 2) {
      NumberOfTicks++;
    }

    for ( ElapsedTick = 0, CurrentTick = GetPerformanceCounter ();
          ElapsedTick <= NumberOfTicks;
          ElapsedTick += GetElapsedTick (&CurrentTick, StartTick, EndTick)
          )
    {
      Status = DtIoReadReg (This, Width, Reg, Offset, 1, Result);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      if ((*Result & Mask) == Value) {
        return EFI_SUCCESS;
      }
    }
  }

  return EFI_TIMEOUT;
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
  IN     EFI_DT_SIZE               Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  )
{
  DT_DEVICE  *DtDevice;
  UINTN      AddressIncrement = Count;

  if ((This == NULL) || (Reg == NULL) || (Buffer == NULL) ||
      (Width >= EfiDtIoWidthMaximum))
  {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  if ((Width >= EfiDtIoWidthFifoUint8) && (Width <= EfiDtIoWidthFifoUint64)) {
    AddressIncrement = 1;
  }

  if (Offset + AddressIncrement * DT_IO_PROTOCOL_WIDTH (Width) >
      Reg->Length)
  {
    return EFI_INVALID_PARAMETER;
  }

  if (Reg->BusDtIo != NULL) {
    if (This == Reg->BusDtIo) {
      if ((DtDevice->Callbacks == NULL) ||
          (DtDevice->Callbacks->WriteChildReg == NULL))
      {
        //
        // The driver didn't expect to be asked to write registers
        // on behalf of a child device.
        //
        return EFI_UNSUPPORTED;
      }

      return DtDevice->Callbacks->WriteChildReg (
                                    Reg->BusDtIo,
                                    Width,
                                    Reg,
                                    Offset,
                                    Count,
                                    Buffer
                                    );
    }

    //
    // Reg->TranslatedBase is not a CPU address - use the
    // parent bus accessor.
    //
    return Reg->BusDtIo->WriteReg (
                           Reg->BusDtIo,
                           Width,
                           Reg,
                           Offset,
                           Count,
                           Buffer
                           );
  }

  //
  // CPU addresses.
  //
  return gCpuIo2->Mem.Write (
                        gCpuIo2,
                        (EFI_CPU_IO_PROTOCOL_WIDTH)Width,
                        Reg->TranslatedBase + Offset,
                        Count,
                        Buffer
                        );
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
  IN     EFI_DT_SIZE               Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  )
{
  DT_DEVICE  *DtDevice;
  UINTN      AddressIncrement = Count;

  if ((This == NULL) || (Reg == NULL) || (Buffer == NULL) ||
      (Width >= EfiDtIoWidthMaximum))
  {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  if ((Width >= EfiDtIoWidthFifoUint8) && (Width <= EfiDtIoWidthFifoUint64)) {
    AddressIncrement = 1;
  }

  if (Offset + AddressIncrement * DT_IO_PROTOCOL_WIDTH (Width) >
      Reg->Length)
  {
    return EFI_INVALID_PARAMETER;
  }

  if (Reg->BusDtIo != NULL) {
    if (This == Reg->BusDtIo) {
      if ((DtDevice->Callbacks == NULL) ||
          (DtDevice->Callbacks->ReadChildReg == NULL))
      {
        //
        // The driver didn't expect to be asked to read registers
        // on behalf of a child device.
        //
        return EFI_UNSUPPORTED;
      }

      return DtDevice->Callbacks->ReadChildReg (
                                    Reg->BusDtIo,
                                    Width,
                                    Reg,
                                    Offset,
                                    Count,
                                    Buffer
                                    );
    }

    //
    // Reg->TranslatedBase is not a CPU address - use the
    // parent bus accessor.
    //
    return Reg->BusDtIo->ReadReg (
                           Reg->BusDtIo,
                           Width,
                           Reg,
                           Offset,
                           Count,
                           Buffer
                           );
  }

  //
  // CPU addresses.
  //
  return gCpuIo2->Mem.Read (
                        gCpuIo2,
                        (EFI_CPU_IO_PROTOCOL_WIDTH)Width,
                        Reg->TranslatedBase + Offset,
                        Count,
                        Buffer
                        );
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
  IN  EFI_DT_SIZE               DestOffset,
  IN  EFI_DT_REG                *SrcReg,
  IN  EFI_DT_SIZE               SrcOffset,
  IN  UINTN                     Count
  )
{
  VOID        *Buffer;
  UINTN       BufferSize;
  EFI_STATUS  Status;

  if ((This == NULL) || (DestReg == NULL) ||
      (SrcReg == NULL) || (Width >= EfiDtIoWidthMaximum))
  {
    return EFI_INVALID_PARAMETER;
  }

  BufferSize = DT_IO_PROTOCOL_WIDTH (Width) * Count;
  if (BufferSize == 0) {
    //
    // We don't want to return EFI_SUCCESS on Count == 0, but
    // instead make sure all the parameter validation happens
    // e.g. in DtIoReadReg and gCpuIo2->Mem.Read. So make sure
    // AllocateZeroPool succeeds.
    //
    BufferSize++;
  }

  /*
   * Consider an implementation that doesn't use a temporary buffer.
   * Of course, it will have to deal with potentially overlapping
   * source/destination regions if DestReg == SrcReg.
   */

  Buffer = AllocateZeroPool (BufferSize);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DtIoReadReg (
             This,
             Width,
             SrcReg,
             SrcOffset,
             Count,
             Buffer
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = DtIoWriteReg (
             This,
             Width,
             DestReg,
             DestOffset,
             Count,
             Buffer
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:
  if (Buffer != NULL) {
    FreePool (Buffer);
  }

  return Status;
}

/**
  Modify the type and UEFI memory attributes for a region described by an
  EFI_DT_REG register space descriptor.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Reg                   EFI_DT_REG *.
  @param  Type                  One of EFI_DT_IO_REG_TYPE.
  @param  MemoryAttributes      Defined in GetMemoryMap() in the UEFI spec.
  @param  OldType               Where to store current type.
  @param  OldAttributes         Where to store current attributes.

  @retval EFI_STATUS            EFI_SUCCESS or error.

**/
EFI_STATUS
EFIAPI
DtIoSetRegType (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_DT_REG          *Reg,
  IN  EFI_DT_IO_REG_TYPE  Type,
  IN  UINT64              MemoryAttributes,
  OUT EFI_DT_IO_REG_TYPE  *OldType OPTIONAL,
  OUT UINT64              *OldAttributes OPTIONAL
  )
{
  EFI_STATUS            Status;
  EFI_GCD_MEMORY_TYPE   GcdType;
  EFI_PHYSICAL_ADDRESS  Base;

  if ((This == NULL) || (Reg == NULL) ||
      (Reg->Length == 0) ||
      (MemoryAttributes == 0))
  {
    return EFI_INVALID_PARAMETER;
  }

  switch (Type) {
    case EfiDtIoRegTypeReserved:
      GcdType = EfiGcdMemoryTypeReserved;
      break;
    case EfiDtIoRegTypeSystemMemory:
      GcdType = EfiGcdMemoryTypeSystemMemory;
      break;
    case EfiDtIoRegTypeMemoryMappedIo:
      GcdType = EfiGcdMemoryTypeMemoryMappedIo;
      break;
    case EfiDtIoRegTypePersistent:
      GcdType = EfiGcdMemoryTypePersistent;
      break;
    case EfiDtIoRegTypeMoreReliable:
      GcdType = EfiGcdMemoryTypeMoreReliable;
      break;
    default:
      return EFI_INVALID_PARAMETER;
  }

  Status = FbpRegToPhysicalAddress (Reg, &Base);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ApplyGcdTypeAndAttrs (
             Base,
             Reg->Length,
             GcdType,
             MemoryAttributes,
             &GcdType,
             OldAttributes,
             FALSE
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (OldType != NULL) {
    switch (GcdType) {
      case EfiGcdMemoryTypeNonExistent:
        *OldType = EfiDtIoRegTypeNonExistent;
        break;
      case EfiGcdMemoryTypeReserved:
        *OldType = EfiDtIoRegTypeReserved;
        break;
      case EfiGcdMemoryTypeSystemMemory:
        *OldType = EfiDtIoRegTypeSystemMemory;
        break;
      case EfiGcdMemoryTypeMemoryMappedIo:
        *OldType = EfiDtIoRegTypeMemoryMappedIo;
        break;
      case EfiGcdMemoryTypePersistent:
        *OldType = EfiDtIoRegTypePersistent;
        break;
      case EfiGcdMemoryTypeMoreReliable:
        *OldType = EfiDtIoRegTypeMoreReliable;
        break;
      default:
        *OldType = EfiDtIoRegTypeInvalid;
        break;
    }
  }

  return Status;
}

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
EFI_STATUS
EFIAPI
DtIoSetCallbacks (
  IN  EFI_DT_IO_PROTOCOL     *This,
  IN  EFI_HANDLE             AgentHandle,
  IN  EFI_DT_IO_PROTOCOL_CB  *Callbacks
  )
{
  BOOLEAN                              Found;
  DT_DEVICE                            *DtDevice;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  Entry;

  if ((This == NULL) || (AgentHandle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);
  Found    = FbpHandleHasBoundDriver (DtDevice->Handle, 0, &Entry);
  ASSERT (Found);
  if (!Found) {
    return EFI_ACCESS_DENIED;
  }

  ASSERT (Entry.AgentHandle == AgentHandle);
  if (Entry.AgentHandle != AgentHandle) {
    return EFI_ACCESS_DENIED;
  }

  if (Callbacks != NULL) {
    ASSERT (DtDevice->Callbacks == NULL);
    if (DtDevice->Callbacks != NULL) {
      return EFI_ACCESS_DENIED;
    }
  }

  DtDevice->Callbacks = Callbacks;
  return EFI_SUCCESS;
}
