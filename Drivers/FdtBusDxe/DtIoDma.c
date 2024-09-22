/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

#define KNOWN_CONSTRAINTS  (EFI_DT_IO_DMA_WITH_MAX_ADDRESS | EFI_DT_IO_DMA_NON_COHERENT)

/**
  Provides the device-specific addresses needed to access system memory.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Operation             Indicates if the bus master is going to read or write to system memory.
  @param  HostAddress           The system memory address to map to the device.
  @param  ExtraConstraints      Additional optional DMA constraints.
  @param  NumberOfBytes         On input the number of bytes to map. On output the number of bytes
                                that were mapped.
  @param  DeviceAddress         The resulting map address for the bus master device to use to access
                                the host's HostAddress.
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
  IN      EFI_DT_IO_PROTOCOL_DMA_EXTRA      *ExtraConstraints OPTIONAL,
  IN  OUT UINTN                             *NumberOfBytes,
  OUT     EFI_DT_BUS_ADDRESS                *DeviceAddress,
  OUT     VOID                              **Mapping
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  MaxAddress;
  EFI_PHYSICAL_ADDRESS  PhysicalAddress;
  MAP_INFO              *MapInfo;
  DT_DEVICE             *DtDevice;
  BOOLEAN               IsCoherent;

  if ((This == NULL) ||
      (Operation >= EfiDtIoDmaOperationMaximum) ||
      (HostAddress == NULL) ||
      (NumberOfBytes == NULL) ||
      (DeviceAddress == NULL) ||
      (Mapping == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  if ((DtDevice->Flags & DT_DEVICE_NON_IDENTITY_DMA) != 0) {
    DEBUG ((DEBUG_ERROR, "%s: non-identity DMA Map is unsupported\n", This->ComponentName));
    return EFI_UNSUPPORTED;
  }

  MaxAddress = DtDevice->MaxCpuDmaAddress;
  IsCoherent = This->IsDmaCoherent;
  if (ExtraConstraints != NULL) {
    if ((ExtraConstraints->Flags & ~KNOWN_CONSTRAINTS) != 0) {
      return EFI_INVALID_PARAMETER;
    }

    if ((ExtraConstraints->Flags & EFI_DT_IO_DMA_WITH_MAX_ADDRESS) != 0) {
      MaxAddress = MIN (MaxAddress, ExtraConstraints->MaxAddress);
    }

    if ((ExtraConstraints->Flags & EFI_DT_IO_DMA_NON_COHERENT) != 0) {
      IsCoherent = FALSE;
    }
  }

  if (!IsCoherent) {
    DEBUG ((DEBUG_ERROR, "%s: non-coherent DMA Map is unsupported\n", This->ComponentName));
    return EFI_UNSUPPORTED;
  }

  PhysicalAddress = (EFI_PHYSICAL_ADDRESS)HostAddress;
  if ((PhysicalAddress + *NumberOfBytes - 1) > MaxAddress) {
    if (Operation == EfiDtIoDmaOperationBusMasterCommonBuffer) {
      //
      // Common buffer operations cannot be remapped.... in the sense
      // that the driver expecting to use common buffer operations won't
      // be calling map/unmap on I/O, so this is a user error. The buffer
      // should have gotten created with the correct constraints and
      // it was not.
      //
      return EFI_INVALID_PARAMETER;
    }

    //
    // Bounce here.
    //
    MapInfo = AllocatePool (sizeof (MAP_INFO));
    if (MapInfo == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      DEBUG ((DEBUG_ERROR, "%a: MAP_INFO: %r\n", Status));
      return Status;
    }

    MapInfo->Signature         = MAP_INFO_SIGNATURE;
    MapInfo->Operation         = Operation;
    MapInfo->NumberOfBytes     = *NumberOfBytes;
    MapInfo->NumberOfPages     = EFI_SIZE_TO_PAGES (*NumberOfBytes);
    MapInfo->HostAddress       = PhysicalAddress;
    MapInfo->MappedHostAddress = MaxAddress;

    Status = gBS->AllocatePages (
                    AllocateMaxAddress,
                    EfiBootServicesData,
                    MapInfo->NumberOfPages,
                    &MapInfo->MappedHostAddress
                    );
    if (EFI_ERROR (Status)) {
      FreePool (MapInfo);
      DEBUG ((DEBUG_ERROR, "%a: AllocatePages: %r\n", Status));
      return Status;
    }

    ZeroMem (
      (VOID *)MapInfo->MappedHostAddress,
      EFI_PAGES_TO_SIZE (MapInfo->NumberOfPages)
      );

    if (Operation == EfiDtIoDmaOperationBusMasterRead) {
      CopyMem (
        (VOID *)MapInfo->MappedHostAddress,
        (VOID *)MapInfo->HostAddress,
        MapInfo->NumberOfBytes
        );
    }

    InsertTailList (&DtDevice->Maps, &MapInfo->Link);

    *DeviceAddress = MapInfo->MappedHostAddress;
    *Mapping       = MapInfo;
    return EFI_SUCCESS;
  }

  *DeviceAddress = PhysicalAddress;
  *Mapping       = NO_MAPPING;

  return EFI_SUCCESS;
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
  MAP_INFO    *MapInfo;
  LIST_ENTRY  *Link;
  DT_DEVICE   *DtDevice;

  if ((This == NULL) || (Mapping == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  if (Mapping == NO_MAPPING) {
    return EFI_SUCCESS;
  }

  MapInfo = NO_MAPPING;
  for (Link = GetFirstNode (&DtDevice->Maps)
       ; !IsNull (&DtDevice->Maps, Link)
       ; Link = GetNextNode (&DtDevice->Maps, Link)
       )
  {
    MapInfo = MAP_INFO_FROM_LINK (Link);
    if (MapInfo == Mapping) {
      break;
    }
  }

  //
  // Mapping is not a valid value returned by Map().
  //
  if (MapInfo != Mapping) {
    return EFI_INVALID_PARAMETER;
  }

  RemoveEntryList (&MapInfo->Link);

  //
  // If this is a write operation from the Bus Master's point of view,
  // then copy the contents of the mapped buffer into the real buffer
  // so the processor can read the contents of the real buffer.
  //
  if (MapInfo->Operation == EfiDtIoDmaOperationBusMasterWrite) {
    CopyMem (
      (VOID *)MapInfo->HostAddress,
      (VOID *)MapInfo->MappedHostAddress,
      MapInfo->NumberOfBytes
      );
  }

  //
  // Free the mapped buffer and the MAP_INFO structure.
  //
  gBS->FreePages (MapInfo->MappedHostAddress, MapInfo->NumberOfPages);
  FreePool (Mapping);

  return EFI_SUCCESS;
}

/**
  Allocates pages that are suitable for an EfiDtIoDmaOperationBusMasterCommonBuffer
  mapping.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  MemoryType            The type of memory to allocate, EfiBootServicesData or
                                EfiRuntimeServicesData.
  @param  Pages                 The number of pages to allocate (> 0).
  @param  ExtraConstraints      Additional optional DMA constraints.
  @param  HostAddress           A pointer to store the base system memory address of the
                                allocated range.

  @retval EFI_SUCCESS           The requested memory pages were allocated.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The memory pages could not be allocated.

**/
EFI_STATUS
EFIAPI
DtIoAllocateBuffer (
  IN  EFI_DT_IO_PROTOCOL            *This,
  IN  EFI_MEMORY_TYPE               MemoryType,
  IN  UINTN                         Pages,
  IN  EFI_DT_IO_PROTOCOL_DMA_EXTRA  *ExtraConstraints OPTIONAL,
  OUT VOID                          **HostAddress
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  Address;
  BOOLEAN               IsCoherent;
  DT_DEVICE             *DtDevice;

  if ((This == NULL) || (Pages == 0) || (HostAddress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  if ((DtDevice->Flags & DT_DEVICE_NON_IDENTITY_DMA) != 0) {
    DEBUG ((DEBUG_ERROR, "%s: non-identity DMA Map is unsupported\n", This->ComponentName));
    return EFI_UNSUPPORTED;
  }

  if ((MemoryType != EfiBootServicesData) &&
      (MemoryType != EfiRuntimeServicesData))
  {
    return EFI_INVALID_PARAMETER;
  }

  Address    = DtDevice->MaxCpuDmaAddress;
  IsCoherent = This->IsDmaCoherent;
  if (ExtraConstraints != NULL) {
    if ((ExtraConstraints->Flags & ~KNOWN_CONSTRAINTS) != 0) {
      return EFI_INVALID_PARAMETER;
    }

    if ((ExtraConstraints->Flags & EFI_DT_IO_DMA_WITH_MAX_ADDRESS) != 0) {
      Address = MIN (Address, ExtraConstraints->MaxAddress);
    }

    if ((ExtraConstraints->Flags & EFI_DT_IO_DMA_NON_COHERENT) != 0) {
      IsCoherent = FALSE;
    }
  }

  if (!IsCoherent) {
    DEBUG ((DEBUG_ERROR, "%s: non-coherent DMA AllocateBuffer is unsupported\n", This->ComponentName));
    return EFI_UNSUPPORTED;
  }

  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  MemoryType,
                  Pages,
                  &Address
                  );
  if (!EFI_ERROR (Status)) {
    ZeroMem (
      (VOID *)Address,
      EFI_PAGES_TO_SIZE (Pages)
      );

    *HostAddress = (VOID *)Address;
  }

  return Status;
}

/**
  Frees memory that was allocated with AllocateBuffer().

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Pages                 The number of pages to free (> 0).
  @param  HostAddress           The base system memory address of the allocated range.

  @retval EFI_SUCCESS           The requested memory pages were freed.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_NOT_FOUND         The memory range specified by HostAddress and Pages
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
  if ((This == NULL) || (Pages == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  return gBS->FreePages ((EFI_PHYSICAL_ADDRESS)HostAddress, Pages);
}
