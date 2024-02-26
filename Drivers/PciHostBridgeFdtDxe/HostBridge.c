/** @file
    DT-based PCI(e) host bridge driver.

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Driver.h"

GLOBAL_REMOVE_IF_UNREFERENCED CHAR8  *mNotifyPhaseTypeStr[] = {
  "BeginEnumeration",        "BeginBusAllocation",    "EndBusAllocation",
  "BeginResourceAllocation", "AllocateResources",     "SetResources",
  "FreeResources",           "EndResourceAllocation", "EndEnumeration"
};

GLOBAL_REMOVE_IF_UNREFERENCED CHAR16  *mPciResourceTypeStr[] = {
  L"I/O", L"Mem", L"PMem", L"Mem64", L"PMem64", L"Bus"
};

GLOBAL_REMOVE_IF_UNREFERENCED CHAR16  *mAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

/**
  Allocate Length of MMIO or IO resource with alignment BitsOfAlignment
  from GCD range [BaseAddress, Limit).

  @param Mmio            TRUE for MMIO and FALSE for IO.
  @param Length          Length of the resource to allocate.
  @param BitsOfAlignment Alignment of the resource to allocate.
  @param BaseAddress     The starting address the allocation is from.
  @param Limit           The ending address the allocation is to.

  @retval  The base address of the allocated resource or MAX_UINT64 if allocation
           fails.
**/
STATIC
UINT64
AllocateResource (
  IN  BOOLEAN  Mmio,
  IN  UINT64   Length,
  IN  UINTN    BitsOfAlignment,
  IN  UINT64   BaseAddress,
  IN  UINT64   Limit
  )
{
  EFI_STATUS  Status;

  if (BaseAddress < Limit) {
    //
    // Have to make sure Aligment is handled since we are doing direct address allocation
    // Strictly speaking, alignment requirement should be applied to device
    // address instead of host address which is used in GCD manipulation below,
    // but as we restrict the alignment of Translation to be larger than any BAR
    // alignment in the root bridge, we can simplify the situation and consider
    // the same alignment requirement is also applied to host address.
    //
    BaseAddress = ALIGN_VALUE (BaseAddress, LShiftU64 (1, BitsOfAlignment));

    while (BaseAddress + Length <= Limit + 1) {
      if (Mmio) {
        Status = gDS->AllocateMemorySpace (
                        EfiGcdAllocateAddress,
                        EfiGcdMemoryTypeMemoryMappedIo,
                        BitsOfAlignment,
                        Length,
                        &BaseAddress,
                        gImageHandle,
                        NULL
                        );
      } else {
        Status = gDS->AllocateIoSpace (
                        EfiGcdAllocateAddress,
                        EfiGcdIoTypeIo,
                        BitsOfAlignment,
                        Length,
                        &BaseAddress,
                        gImageHandle,
                        NULL
                        );
      }

      if (!EFI_ERROR (Status)) {
        return BaseAddress;
      }

      BaseAddress += LShiftU64 (1, BitsOfAlignment);
    }
  }

  return MAX_UINT64;
}

/**

  Enter a certain phase of the PCI enumeration process.

  @param This   The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL instance.
  @param Phase  The phase during enumeration.

  @retval EFI_SUCCESS            Succeed.
  @retval EFI_INVALID_PARAMETER  Wrong phase parameter passed in.
  @retval EFI_NOT_READY          Resources have not been submitted yet.

**/
STATIC
EFI_STATUS
EFIAPI
HostBridgeNotifyPhase (
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE     Phase
  )
{
  EFI_STATUS                ReturnStatus;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;

  if ((This == NULL) || (Phase >= EfiMaxPciHostBridgeEnumerationPhase)) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = PCI_ROOT_BRIDGE_FROM_RES_ALLOC (This);
  DEBUG ((
    DEBUG_INFO,
    "%s: NotifyPhase(%a)\n",
    RootBridge->DevicePathStr,
    mNotifyPhaseTypeStr[Phase]
    ));

  switch (Phase) {
    case EfiPciHostBridgeBeginEnumeration:
    {
      UINTN  Index;

      if (!RootBridge->CanRestart) {
        return EFI_NOT_READY;
      }

      for (Index = TypeIo; Index < TypeMax; Index++) {
        RootBridge->ResAllocNode[Index].Base      = 0;
        RootBridge->ResAllocNode[Index].Length    = 0;
        RootBridge->ResAllocNode[Index].Alignment = 0;
        RootBridge->ResAllocNode[Index].Status    = ResNone;

        RootBridge->ResourceSubmitted = FALSE;
      }

      break;
    }
    case EfiPciHostBridgeBeginBusAllocation:
      //
      // No specific action is required here, can perform any chipset specific programming.
      //
      RootBridge->CanRestart = FALSE;
      break;
    case EfiPciHostBridgeEndBusAllocation:
      //
      // No specific action is required here, can perform any chipset specific programming.
      //
      break;
    case EfiPciHostBridgeBeginResourceAllocation:
      //
      // No specific action is required here, can perform any chipset specific programming.
      //
      break;
    case EfiPciHostBridgeAllocateResources:
    {
      UINTN                 Index;
      UINTN                 Index1;
      UINTN                 Index2;
      EFI_PHYSICAL_ADDRESS  BaseAddress;
      UINTN                 BitsOfAlignment;
      UINT64                Alignment;
      UINT64                MaxAlignment;
      UINT64                Translation;
      BOOLEAN               ResNodeHandled[TypeMax];
      ReturnStatus = EFI_SUCCESS;

      if (!RootBridge->ResourceSubmitted) {
        return EFI_NOT_READY;
      }

      for (Index = TypeIo; Index < TypeBus; Index++) {
        ResNodeHandled[Index] = FALSE;
      }

      for (Index1 = TypeIo; Index1 < TypeBus; Index1++) {
        if (RootBridge->ResAllocNode[Index1].Status == ResNone) {
          ResNodeHandled[Index1] = TRUE;
        } else {
          //
          // Allocate the resource node with max alignment at first.
          //
          MaxAlignment = 0;
          Index        = TypeMax;
          for (Index2 = TypeIo; Index2 < TypeBus; Index2++) {
            if (ResNodeHandled[Index2]) {
              continue;
            }

            if (MaxAlignment <= RootBridge->ResAllocNode[Index2].Alignment) {
              MaxAlignment = RootBridge->ResAllocNode[Index2].Alignment;
              Index        = Index2;
            }
          }

          ASSERT (Index < TypeMax);
          ResNodeHandled[Index] = TRUE;
          Alignment             = RootBridge->ResAllocNode[Index].Alignment;
          BitsOfAlignment       = LowBitSet64 (Alignment + 1);
          BaseAddress           = MAX_UINT64;

          //
          // RESTRICTION: To simplify the situation, we require the alignment of
          // Translation must be larger than any BAR alignment in the same root
          // bridge, so that resource allocation alignment can be applied to
          // both device address and host address.
          //
          Translation = GetTranslationByResourceType (RootBridge, Index);
          if ((Translation & Alignment) != 0) {
            DEBUG ((
              DEBUG_ERROR,
              "%s: Translation 0x%lx is not aligned to 0x%lx!\n",
              RootBridge->DevicePathStr,
              Translation,
              Alignment
              ));
            ASSERT ((Translation & Alignment) == 0);
            //
            // This may be caused by too large alignment or too small
            // Translation; pick the 1st possibility and return out of resource,
            // which can also go thru the same process for out of resource
            // outside the loop.
            //
            ReturnStatus = EFI_OUT_OF_RESOURCES;
            continue;
          }

          switch (Index) {
            case TypeIo:
              //
              // Base and Limit in PCI_ROOT_BRIDGE_APERTURE are device address.
              // For AllocateResource is manipulating GCD resource, we need to use
              // host address here.
              //
              BaseAddress = AllocateResource (
                              FALSE,
                              RootBridge->ResAllocNode[Index].Length,
                              MIN (15, BitsOfAlignment),
                              TO_HOST_ADDRESS (
                                ALIGN_VALUE (RB (RootBridge->IoRange), Alignment + 1),
                                RT (RootBridge->IoRange)
                                ),
                              TO_HOST_ADDRESS (
                                RL (RootBridge->IoRange),
                                RT (RootBridge->IoRange)
                                )
                              );
              break;

            case TypeMem64:
              BaseAddress = AllocateResource (
                              TRUE,
                              RootBridge->ResAllocNode[Index].Length,
                              MIN (63, BitsOfAlignment),
                              TO_HOST_ADDRESS (
                                ALIGN_VALUE (RB (RootBridge->MemAbove4GRange), Alignment + 1),
                                RT (RootBridge->MemAbove4GRange)
                                ),
                              TO_HOST_ADDRESS (
                                RL (RootBridge->MemAbove4GRange),
                                RT (RootBridge->MemAbove4GRange)
                                )
                              );
              if (BaseAddress != MAX_UINT64) {
                break;
              }

            //
            // If memory above 4GB is not available, try memory below 4GB.
            //
            case TypeMem32:
              BaseAddress = AllocateResource (
                              TRUE,
                              RootBridge->ResAllocNode[Index].Length,
                              MIN (31, BitsOfAlignment),
                              TO_HOST_ADDRESS (
                                ALIGN_VALUE (RB (RootBridge->MemRange), Alignment + 1),
                                RT (RootBridge->MemRange)
                                ),
                              TO_HOST_ADDRESS (
                                RL (RootBridge->MemRange),
                                RT (RootBridge->MemRange)
                                )
                              );
              break;

            case TypePMem64:
              BaseAddress = AllocateResource (
                              TRUE,
                              RootBridge->ResAllocNode[Index].Length,
                              MIN (63, BitsOfAlignment),
                              TO_HOST_ADDRESS (
                                ALIGN_VALUE (RB (RootBridge->PMemAbove4GRange), Alignment + 1),
                                RT (RootBridge->PMemAbove4GRange)
                                ),
                              TO_HOST_ADDRESS (
                                RL (RootBridge->PMemAbove4GRange),
                                RT (RootBridge->PMemAbove4GRange)
                                )
                              );
              if (BaseAddress != MAX_UINT64) {
                break;
              }

            //
            // If memory above 4GB is not available, try memory below 4GB.
            //
            case TypePMem32:
              BaseAddress = AllocateResource (
                              TRUE,
                              RootBridge->ResAllocNode[Index].Length,
                              MIN (31, BitsOfAlignment),
                              TO_HOST_ADDRESS (
                                ALIGN_VALUE (RB (RootBridge->PMemRange), Alignment + 1),
                                RT (RootBridge->PMemRange)
                                ),
                              TO_HOST_ADDRESS (
                                RL (RootBridge->PMemRange),
                                RT (RootBridge->PMemRange)
                                )
                              );
              break;

            default:
              ASSERT (FALSE);
              break;
          }

          DEBUG ((
            DEBUG_INFO,
            "  %s: Base/Length/Alignment = %lx/%lx/%lx - ",
            mPciResourceTypeStr[Index],
            BaseAddress,
            RootBridge->ResAllocNode[Index].Length,
            Alignment
            ));
          if (BaseAddress != MAX_UINT64) {
            RootBridge->ResAllocNode[Index].Base   = BaseAddress;
            RootBridge->ResAllocNode[Index].Status = ResAllocated;
            DEBUG ((DEBUG_INFO, "%r\n", ReturnStatus));
          } else {
            ReturnStatus = EFI_OUT_OF_RESOURCES;
            DEBUG ((DEBUG_ERROR, "%r\n", ReturnStatus));
          }
        }
      }

      //
      // Set resource to zero for nodes where allocation fails.
      //
      for (Index = TypeIo; Index < TypeBus; Index++) {
        if (RootBridge->ResAllocNode[Index].Status != ResAllocated) {
          RootBridge->ResAllocNode[Index].Length = 0;
        }
      }

      return ReturnStatus;
    }
    case EfiPciHostBridgeSetResources:
      break;

    case EfiPciHostBridgeFreeResources:
    {
      UINTN  Index;

      for (Index = TypeIo; Index < TypeBus; Index++) {
        if (RootBridge->ResAllocNode[Index].Status == ResAllocated) {
          switch (Index) {
            case TypeIo:
              ReturnStatus = gDS->FreeIoSpace (
                                    RootBridge->ResAllocNode[Index].Base,
                                    RootBridge->ResAllocNode[Index].Length
                                    );
              if (EFI_ERROR (ReturnStatus)) {
                DEBUG ((
                  DEBUG_ERROR,
                  "%s: FreeIoSpace(0x%lx-0x%lx): %r\n",
                  RootBridge->DevicePathStr,
                  RootBridge->ResAllocNode[Index].Base,
                  RootBridge->ResAllocNode[Index].Base +
                  RootBridge->ResAllocNode[Index].Length - 1,
                  ReturnStatus
                  ));
                return ReturnStatus;
              }

              break;
            case TypeMem32:
            case TypePMem32:
            case TypeMem64:
            case TypePMem64:
              ReturnStatus = gDS->FreeMemorySpace (
                                    RootBridge->ResAllocNode[Index].Base,
                                    RootBridge->ResAllocNode[Index].Length
                                    );
              if (EFI_ERROR (ReturnStatus)) {
                DEBUG ((
                  DEBUG_ERROR,
                  "%s: FreeMemorySpace(0x%lx-0x%lx): %r\n",
                  RootBridge->DevicePathStr,
                  RootBridge->ResAllocNode[Index].Base,
                  RootBridge->ResAllocNode[Index].Base +
                  RootBridge->ResAllocNode[Index].Length - 1,
                  ReturnStatus
                  ));
                return ReturnStatus;
              }

              break;
            default:
              ASSERT (FALSE);
              break;
          }

          RootBridge->ResAllocNode[Index].Base      = 0;
          RootBridge->ResAllocNode[Index].Length    = 0;
          RootBridge->ResAllocNode[Index].Alignment = 0;
          RootBridge->ResAllocNode[Index].Status    = ResNone;
        }
      }

      RootBridge->ResourceSubmitted = FALSE;
      RootBridge->CanRestart        = TRUE;
      return ReturnStatus;
    }
    case EfiPciHostBridgeEndResourceAllocation:
      //
      // The resource allocation phase is completed.  No specific action is required
      // here. This notification can be used to perform any chipset specific programming.
      //
      break;
    case EfiPciHostBridgeEndEnumeration:
      //
      // The Host Bridge Enumeration is completed. No specific action is required here.
      // This notification can be used to perform any chipset specific programming.
      //
      break;
    default:
      return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**

  Return the device handle of the next PCI root bridge that is associated with
  this Host Bridge.

  @param This              The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ PROTOCOL instance.
  @param RootBridgeHandle  Returns the device handle of the next PCI Root Bridge.
                           On input, it holds the RootBridgeHandle returned by the most
                           recent call to GetNextRootBridge().The handle for the first
                           PCI Root Bridge is returned if RootBridgeHandle is NULL on input.

  @retval EFI_SUCCESS            Succeed.
  @retval EFI_NOT_FOUND          Next PCI root bridge not found.
  @retval EFI_INVALID_PARAMETER  Wrong parameter passed in.

**/
STATIC
EFI_STATUS
EFIAPI
HostBridgeGetNextRootBridge (
  IN     EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN OUT EFI_HANDLE                                        *RootBridgeHandle
  )
{
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;

  if ((This == NULL) || (RootBridgeHandle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = PCI_ROOT_BRIDGE_FROM_RES_ALLOC (This);

  if (*RootBridgeHandle == NULL) {
    *RootBridgeHandle = RootBridge->Controller;
    return EFI_SUCCESS;
  } else if (*RootBridgeHandle != RootBridge->Controller) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_NOT_FOUND;
}

/**

  Returns the attributes of a PCI Root Bridge.

  @param This              The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ PROTOCOL instance
  @param RootBridgeHandle  The device handle of the PCI Root Bridge
                              that the caller is interested in
  @param Attributes        The pointer to attributes of the PCI Root Bridge

  @retval EFI_SUCCESS            Succeed.
  @retval EFI_INVALID_PARAMETER  Attributes parameter passed in is NULL or
                            @retval RootBridgeHandle is not an EFI_HANDLE
                            @retval that was returned on a previous call to
                            @retval GetNextRootBridge().

**/
STATIC
EFI_STATUS
EFIAPI
HostBridgeGetAttributes (
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN  EFI_HANDLE                                        RootBridgeHandle,
  OUT UINT64                                            *Attributes
  )
{
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;

  if ((This == NULL) || (Attributes == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = PCI_ROOT_BRIDGE_FROM_RES_ALLOC (This);
  if (RootBridge->Controller != RootBridgeHandle) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%s: GetAttributes\n", RootBridge->DevicePathStr));
  *Attributes = RootBridge->AllocationAttributes;
  return EFI_SUCCESS;
}

/**

  This is the request from the PCI enumerator to set up
  the specified PCI Root Bridge for bus enumeration process.

  @param This              The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ PROTOCOL instance.
  @param RootBridgeHandle  The PCI Root Bridge to be set up.
  @param Configuration     Pointer to the pointer to the PCI bus resource descriptor.

  @retval EFI_SUCCESS            Succeed.
  @retval EFI_OUT_OF_RESOURCES   Not enough pool to be allocated.
  @retval EFI_INVALID_PARAMETER  RootBridgeHandle is not a valid handle.

**/
STATIC
EFI_STATUS
EFIAPI
HostBridgeStartBusEnumeration (
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN  EFI_HANDLE                                        RootBridgeHandle,
  OUT VOID                                              **Configuration
  )
{
  PCI_ROOT_BRIDGE_INSTANCE           *RootBridge;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Descriptor;
  EFI_ACPI_END_TAG_DESCRIPTOR        *End;

  if ((This == NULL) || (Configuration == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = PCI_ROOT_BRIDGE_FROM_RES_ALLOC (This);
  if (RootBridge->Controller != RootBridgeHandle) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%s: StartBusNumbers\n", RootBridge->DevicePathStr));
  *Configuration = AllocatePool (sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR));
  if (*Configuration == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Descriptor                        = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)*Configuration;
  Descriptor->Desc                  = ACPI_ADDRESS_SPACE_DESCRIPTOR;
  Descriptor->Len                   = sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) - 3;
  Descriptor->ResType               = ACPI_ADDRESS_SPACE_TYPE_BUS;
  Descriptor->GenFlag               = 0;
  Descriptor->SpecificFlag          = 0;
  Descriptor->AddrSpaceGranularity  = 0;
  Descriptor->AddrRangeMin          = RB (RootBridge->BusRange);
  Descriptor->AddrRangeMax          = 0;
  Descriptor->AddrTranslationOffset = 0;
  Descriptor->AddrLen               = RS (RootBridge->BusRange);

  End           = (EFI_ACPI_END_TAG_DESCRIPTOR *)(Descriptor + 1);
  End->Desc     = ACPI_END_TAG_DESCRIPTOR;
  End->Checksum = 0x0;

  return EFI_SUCCESS;
}

/**

  This function programs the PCI Root Bridge hardware so that
  it decodes the specified PCI bus range.

  @param This              The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ PROTOCOL instance.
  @param RootBridgeHandle  The PCI Root Bridge whose bus range is to be programmed.
  @param Configuration     The pointer to the PCI bus resource descriptor.

  @retval EFI_SUCCESS            Succeed.
  @retval EFI_INVALID_PARAMETER  Wrong parameters passed in.

**/
STATIC
EFI_STATUS
EFIAPI
HostBridgeSetBusNumbers (
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN EFI_HANDLE                                        RootBridgeHandle,
  IN VOID                                              *Configuration
  )
{
  PCI_ROOT_BRIDGE_INSTANCE           *RootBridge;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Descriptor;
  EFI_ACPI_END_TAG_DESCRIPTOR        *End;

  if ((This == NULL) || (Configuration == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = PCI_ROOT_BRIDGE_FROM_RES_ALLOC (This);
  if (RootBridge->Controller != RootBridgeHandle) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%s: SetBusNumbers\n", RootBridge->DevicePathStr));
  Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Configuration;
  End        = (EFI_ACPI_END_TAG_DESCRIPTOR *)(Descriptor + 1);

  //
  // Check the Configuration is valid
  //
  if ((Descriptor->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR) ||
      (Descriptor->ResType != ACPI_ADDRESS_SPACE_TYPE_BUS) ||
      (End->Desc != ACPI_END_TAG_DESCRIPTOR)
      )
  {
    return EFI_INVALID_PARAMETER;
  }

  if (Descriptor->AddrLen == 0) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Descriptor->AddrRangeMin < RB (RootBridge->BusRange)) ||
      (Descriptor->AddrRangeMin + Descriptor->AddrLen - 1 > RL (RootBridge->BusRange)))
  {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Update the Bus Range
  //
  RootBridge->ResAllocNode[TypeBus].Base   = Descriptor->AddrRangeMin;
  RootBridge->ResAllocNode[TypeBus].Length = Descriptor->AddrLen;
  RootBridge->ResAllocNode[TypeBus].Status = ResAllocated;
  return EFI_SUCCESS;
}

/**

  Submits the I/O and memory resource requirements for the specified PCI Root Bridge.

  @param This              The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ PROTOCOL instance
  @param RootBridgeHandle  The PCI Root Bridge whose I/O and memory resource requirements
                              are being submitted
  @param Configuration     The pointer to the PCI I/O and PCI memory resource descriptor

  @retval EFI_SUCCESS            Succeed.
  @retval EFI_INVALID_PARAMETER  Wrong parameters passed in.

**/
STATIC
EFI_STATUS
EFIAPI
HostBridgeSubmitResources (
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN EFI_HANDLE                                        RootBridgeHandle,
  IN VOID                                              *Configuration
  )
{
  PCI_ROOT_BRIDGE_INSTANCE           *RootBridge;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Descriptor;
  PCI_RESOURCE_TYPE                  Type;

  if ((This == NULL) || (Configuration == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = PCI_ROOT_BRIDGE_FROM_RES_ALLOC (This);
  if (RootBridge->Controller != RootBridgeHandle) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%s: SubmitResources\n", RootBridge->DevicePathStr));

  //
  // Check the resource descriptors.
  // If the Configuration includes one or more invalid resource descriptors, all the resource
  // descriptors are ignored and the function returns EFI_INVALID_PARAMETER.
  //
  for (Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Configuration;
       Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR;
       Descriptor++)
  {
    if (Descriptor->ResType > ACPI_ADDRESS_SPACE_TYPE_BUS) {
      return EFI_INVALID_PARAMETER;
    }

    DEBUG ((
      DEBUG_INFO,
      " %s: Granularity/SpecificFlag = %ld / %02x%s\n",
      mAcpiAddressSpaceTypeStr[Descriptor->ResType],
      Descriptor->AddrSpaceGranularity,
      Descriptor->SpecificFlag,
      (Descriptor->SpecificFlag & EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE) != 0 ? L" (Prefetchable)" : L""
      ));
    DEBUG ((DEBUG_INFO, "      Length/Alignment = 0x%lx / 0x%lx\n", Descriptor->AddrLen, Descriptor->AddrRangeMax));
    switch (Descriptor->ResType) {
      case ACPI_ADDRESS_SPACE_TYPE_MEM:
        if ((Descriptor->AddrSpaceGranularity != 32) && (Descriptor->AddrSpaceGranularity != 64)) {
          return EFI_INVALID_PARAMETER;
        }

        if ((Descriptor->AddrSpaceGranularity == 32) && (Descriptor->AddrLen >= SIZE_4GB)) {
          return EFI_INVALID_PARAMETER;
        }

        //
        // If the PCI root bridge does not support separate windows for nonprefetchable and
        // prefetchable memory, then the PCI bus driver needs to include requests for
        // prefetchable memory in the nonprefetchable memory pool.
        //
        if (((RootBridge->AllocationAttributes & EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM) != 0) &&
            ((Descriptor->SpecificFlag & EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE) != 0)
            )
        {
          return EFI_INVALID_PARAMETER;
        }

      case ACPI_ADDRESS_SPACE_TYPE_IO:
        //
        // Check aligment, it should be of the form 2^n-1
        //
        if (GetPowerOfTwo64 (Descriptor->AddrRangeMax + 1) != (Descriptor->AddrRangeMax + 1)) {
          return EFI_INVALID_PARAMETER;
        }

        break;
      default:
        ASSERT (FALSE);
        break;
    }
  }

  if (Descriptor->Desc != ACPI_END_TAG_DESCRIPTOR) {
    return EFI_INVALID_PARAMETER;
  }

  for (Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Configuration; Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR; Descriptor++) {
    if (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {
      if (Descriptor->AddrSpaceGranularity == 32) {
        if ((Descriptor->SpecificFlag & EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE) != 0) {
          Type = TypePMem32;
        } else {
          Type = TypeMem32;
        }
      } else {
        ASSERT (Descriptor->AddrSpaceGranularity == 64);
        if ((Descriptor->SpecificFlag & EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE) != 0) {
          Type = TypePMem64;
        } else {
          Type = TypeMem64;
        }
      }
    } else {
      ASSERT (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_IO);
      Type = TypeIo;
    }

    RootBridge->ResAllocNode[Type].Length    = Descriptor->AddrLen;
    RootBridge->ResAllocNode[Type].Alignment = Descriptor->AddrRangeMax;
    RootBridge->ResAllocNode[Type].Status    = ResSubmitted;
  }

  RootBridge->ResourceSubmitted = TRUE;
  return EFI_SUCCESS;
}

/**

  This function returns the proposed resource settings for the specified
  PCI Root Bridge.

  @param This              The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ PROTOCOL instance.
  @param RootBridgeHandle  The PCI Root Bridge handle.
  @param Configuration     The pointer to the pointer to the PCI I/O
                              and memory resource descriptor.

  @retval EFI_SUCCESS            Succeed.
  @retval EFI_OUT_OF_RESOURCES   Not enough pool to be allocated.
  @retval EFI_INVALID_PARAMETER  RootBridgeHandle is not a valid handle.

**/
STATIC
EFI_STATUS
EFIAPI
HostBridgeGetProposedResources (
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN  EFI_HANDLE                                        RootBridgeHandle,
  OUT VOID                                              **Configuration
  )
{
  PCI_ROOT_BRIDGE_INSTANCE           *RootBridge;
  UINTN                              Index;
  UINTN                              Number;
  VOID                               *Buffer;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Descriptor;
  EFI_ACPI_END_TAG_DESCRIPTOR        *End;
  UINT64                             ResStatus;

  if ((This == NULL) || (Configuration == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = PCI_ROOT_BRIDGE_FROM_RES_ALLOC (This);
  if (RootBridge->Controller != RootBridgeHandle) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%s: GetProposedResources\n", RootBridge->DevicePathStr));

  for (Index = 0, Number = 0; Index < TypeBus; Index++) {
    if (RootBridge->ResAllocNode[Index].Status != ResNone) {
      Number++;
    }
  }

  Buffer = AllocateZeroPool (Number * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR));
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Buffer;
  for (Index = 0; Index < TypeBus; Index++) {
    ResStatus = RootBridge->ResAllocNode[Index].Status;
    if (ResStatus != ResNone) {
      Descriptor->Desc    = ACPI_ADDRESS_SPACE_DESCRIPTOR;
      Descriptor->Len     = sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) - 3;
      Descriptor->GenFlag = 0;
      //
      // AddrRangeMin in Resource Descriptor here should be device address
      // instead of host address, or else PCI bus driver cannot set correct
      // address into PCI BAR registers.
      // Base in ResAllocNode is a host address, so conversion is needed.
      //
      Descriptor->AddrRangeMin = TO_DEVICE_ADDRESS (
                                   RootBridge->ResAllocNode[Index].Base,
                                   GetTranslationByResourceType (RootBridge, Index)
                                   );
      Descriptor->AddrRangeMax          = 0;
      Descriptor->AddrTranslationOffset = (ResStatus == ResAllocated) ? EFI_RESOURCE_SATISFIED : PCI_RESOURCE_LESS;
      Descriptor->AddrLen               = RootBridge->ResAllocNode[Index].Length;

      switch (Index) {
        case TypeIo:
          Descriptor->ResType = ACPI_ADDRESS_SPACE_TYPE_IO;
          break;

        case TypePMem32:
          Descriptor->SpecificFlag = EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE;
        case TypeMem32:
          Descriptor->ResType              = ACPI_ADDRESS_SPACE_TYPE_MEM;
          Descriptor->AddrSpaceGranularity = 32;
          break;

        case TypePMem64:
          Descriptor->SpecificFlag = EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE;
        case TypeMem64:
          Descriptor->ResType              = ACPI_ADDRESS_SPACE_TYPE_MEM;
          Descriptor->AddrSpaceGranularity = 64;
          break;
      }

      Descriptor++;
    }
  }

  End           = (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor;
  End->Desc     = ACPI_END_TAG_DESCRIPTOR;
  End->Checksum = 0;

  *Configuration = Buffer;

  return EFI_SUCCESS;
}

/**

  This function is called for all the PCI controllers that the PCI
  bus driver finds. Can be used to Preprogram the controller.

  @param This              The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ PROTOCOL instance.
  @param RootBridgeHandle  The PCI Root Bridge handle.
  @param PciAddress        Address of the controller on the PCI bus.
  @param Phase             The Phase during resource allocation.

  @retval EFI_SUCCESS            Succeed.
  @retval EFI_INVALID_PARAMETER  RootBridgeHandle is not a valid handle.

**/
STATIC
EFI_STATUS
EFIAPI
HostBridgePreprocessController (
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN EFI_HANDLE                                        RootBridgeHandle,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS       PciAddress,
  IN EFI_PCI_CONTROLLER_RESOURCE_ALLOCATION_PHASE      Phase
  )
{
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;

  if ((This == NULL) || (Phase > EfiPciBeforeResourceCollection)) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = PCI_ROOT_BRIDGE_FROM_RES_ALLOC (This);
  if (RootBridge->Controller != RootBridgeHandle) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%s: PreprocessController\n", RootBridge->DevicePathStr));
  return EFI_SUCCESS;
}

/**

  Perform the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
  side of initization.

  @param RootBridge             PCI_ROOT_BRIDGE_INSTANCE *.

**/
VOID
HostBridgeInit (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge
  )
{
  ASSERT (RootBridge != NULL);

  RootBridge->CanRestart                    = TRUE;
  RootBridge->ResAlloc.NotifyPhase          = HostBridgeNotifyPhase;
  RootBridge->ResAlloc.GetNextRootBridge    = HostBridgeGetNextRootBridge;
  RootBridge->ResAlloc.GetAllocAttributes   = HostBridgeGetAttributes;
  RootBridge->ResAlloc.StartBusEnumeration  = HostBridgeStartBusEnumeration;
  RootBridge->ResAlloc.SetBusNumbers        = HostBridgeSetBusNumbers;
  RootBridge->ResAlloc.SubmitResources      = HostBridgeSubmitResources;
  RootBridge->ResAlloc.GetProposedResources = HostBridgeGetProposedResources;
  RootBridge->ResAlloc.PreprocessController = HostBridgePreprocessController;
}
