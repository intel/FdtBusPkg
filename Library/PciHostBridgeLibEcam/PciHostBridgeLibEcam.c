/** @file
    PCI Host Bridge Library instance for pci-host-ecam-generic
    compatible RC implementations.

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <IndustryStandard/Pci22.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/DtIo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FbpUtilsLib.h>
#include <Library/FbpPciUtilsLib.h>

#pragma pack(1)
typedef struct {
  ACPI_HID_DEVICE_PATH        AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    EndDevicePath;
} MY_PCI_ROOT_BRIDGE_DEVICE_PATH;
#pragma pack ()

MY_PCI_ROOT_BRIDGE_DEVICE_PATH  mRootBridgeDevicePathTemplate = {
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_DP,
      {
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH) >> 8)
      }
    },
    EISA_PNP_ID (0x0A08), // PCI Express
    0
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      END_DEVICE_PATH_LENGTH,
      0
    }
  }
};

GLOBAL_REMOVE_IF_UNREFERENCED
CHAR16  *mPciHostBridgeLibAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

/**
  Process a compatible DtIo into a PCI_ROOT_BRIDGE.

  @param  DtIo                  For a pci-host-ecam-generic node.
  @param  Bridge                PCI_ROOT_BRIDGE to fill.

  @return EFI_STATUS            EFI_SUCCESS or others.
**/
STATIC
EFI_STATUS
EFIAPI
ProcessPciHost (
  IN  EFI_DT_IO_PROTOCOL  *DtIo,
  IN  PCI_ROOT_BRIDGE     *Bridge
  )
{
  EFI_STATUS                Status;
  UINT32                    BusMin;
  UINT32                    BusMax;
  PCI_ROOT_BRIDGE_APERTURE  Io;
  PCI_ROOT_BRIDGE_APERTURE  Mem;
  PCI_ROOT_BRIDGE_APERTURE  MemAbove4G;
  PCI_ROOT_BRIDGE_APERTURE  PMem;
  PCI_ROOT_BRIDGE_APERTURE  PMemAbove4G;
  EFI_DT_RANGE              Range;
  UINTN                     Index;
  EFI_DT_RANGE              DmaRanges;
  EFI_DT_REG                Reg;
  UINT64                    Attributes;
  UINT64                    AllocationAttributes;
  EFI_PHYSICAL_ADDRESS      EcamBase;

  //
  // Have a DT node that looks like:
  //    pci@2c000000 {
  //            reg = <0x0 0x2C000000 0x0 0x1000000
  //                   0x9 0xc0000000 0x0 0x10000000>;
  //            reg-names = "reg", "config";
  //            ranges = <0x82000000  0x0 0x40000000  0x0 0x40000000 0x0 0x40000000>,
  //                     <0xc3000000  0x9 0x80000000  0x9 0x80000000 0x0 0x40000000>;
  //            dma-coherent;
  //            bus-range = <0x00 0xff>;
  //            linux,pci-domain = <0x00>;
  //            device_type = "pci";
  //            compatible = "pci-host-ecam-generic";
  //            #size-cells = <0x02>;
  //            #address-cells = <0x03>;
  //    };

  BusMin = 0;
  BusMax = 0;
  ZeroMem (&Io, sizeof (Io));
  ZeroMem (&Mem, sizeof (Mem));
  ZeroMem (&MemAbove4G, sizeof (MemAbove4G));
  ZeroMem (&PMem, sizeof (PMem));
  ZeroMem (&PMemAbove4G, sizeof (PMemAbove4G));
  Io.Base          = 1;
  Mem.Base         = 1;
  PMem.Base        = 1;
  MemAbove4G.Base  = MAX_UINT64;
  PMemAbove4G.Base = MAX_UINT64;

  for (Index = 0,
       Status = DtIo->GetRange (DtIo, "ranges", Index, &Range);
       !EFI_ERROR (Status);
       Status = DtIo->GetRange (DtIo, "ranges", ++Index, &Range))
  {
    EFI_DT_CELL           SpaceCode;
    EFI_PHYSICAL_ADDRESS  RangeCpuBase;

    Status = FbpRangeToPhysicalAddress (&Range, &RangeCpuBase);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: couldn't translate range[%lu] to CPU addresses: %r\n",
        __func__,
        Index,
        Status
        ));
      ASSERT_EFI_ERROR (Status);
      continue;
    }

    ASSERT (RangeCpuBase == Range.ParentBase);

    SpaceCode = FbpPciGetRangeAttribute (DtIo, Range.ChildBase);
    switch (SpaceCode) {
      case EFI_DT_PCI_HOST_RANGE_IO:
        Io.Base        = Range.ChildBase;
        Io.Limit       = Io.Base + Range.Length - 1;
        Io.Translation = Io.Base - RangeCpuBase;
        break;

        if ((Io.Base > MAX_UINT32) || (Io.Limit > MAX_UINT32)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: skipping invalid IO space [0x%lx-0x%Lx]\n",
            Io.Base,
            Io.Limit,
            __func__
            ));
          Io.Base  = 1;
          Io.Limit = 0;
          break;
        }

      case EFI_DT_PCI_HOST_RANGE_MMIO32:
        Mem.Base        = Range.ChildBase;
        Mem.Limit       = Mem.Base + Range.Length - 1;
        Mem.Translation = Mem.Base - RangeCpuBase;

        if ((Mem.Base > MAX_UINT32) || (Mem.Limit > MAX_UINT32)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: skipping invalid MMIO32 space [0x%lx-0x%Lx]\n",
            Mem.Base,
            Mem.Limit,
            __func__
            ));
          Mem.Base  = 1;
          Mem.Limit = 0;
          break;
        }

        break;
      case EFI_DT_PCI_HOST_RANGE_MMIO32 | EFI_DT_PCI_HOST_RANGE_PREFETCHABLE:
        PMem.Base        = Range.ChildBase;
        PMem.Limit       = PMem.Base + Range.Length - 1;
        PMem.Translation = PMem.Base - RangeCpuBase;

        if ((PMem.Base > MAX_UINT32) || (PMem.Limit > MAX_UINT32)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: skipping invalid prefetch MMIO32 space [0x%lx-0x%Lx]\n",
            PMem.Base,
            PMem.Limit,
            __func__
            ));
          PMem.Base  = 1;
          PMem.Limit = 0;
          break;
        }

        break;
      case EFI_DT_PCI_HOST_RANGE_MMIO64:
        MemAbove4G.Base        = Range.ChildBase;
        MemAbove4G.Limit       = MemAbove4G.Base + Range.Length - 1;
        MemAbove4G.Translation = MemAbove4G.Base - RangeCpuBase;

        break;
      case EFI_DT_PCI_HOST_RANGE_MMIO64 | EFI_DT_PCI_HOST_RANGE_PREFETCHABLE:
        PMemAbove4G.Base        = Range.ChildBase;
        PMemAbove4G.Limit       = PMemAbove4G.Base + Range.Length - 1;
        PMemAbove4G.Translation = PMemAbove4G.Base - RangeCpuBase;

        break;
      default:
        //
        // Don't know what to do with EFI_DT_PCI_HOST_RANGE_RELOCATABLE or
        // EFI_DT_PCI_HOST_RANGE_ALIASED, or if they are even expected.
        //
        DEBUG ((
          DEBUG_ERROR,
          "%a: Unknown SpaceCode 0x%x is detected\n",
          __func__,
          SpaceCode
          ));
        break;
    }
  }

  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DtIoGetRange: %r\n",
      __func__,
      Status
      ));
    return Status;
  }

  //
  // Locate 'config'. Getting the reg ensures it's mapped
  // for access.
  //
  Status = DtIo->GetRegByName (DtIo, "config", &Reg);
  if (EFI_ERROR (Status)) {
    //
    // Not every compatible node will use
    // reg-names, so just treat reg[0] as the ECAM window.
    //
    Status = DtIo->GetReg (DtIo, 0, &Reg);
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: couldn't get the ECAM window\n",
      __func__
      ));
    return Status;
  }

  Status = FbpRegToPhysicalAddress (&Reg, &EcamBase);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: couldn't translate ECAM range to CPU addresses: %r\n",
      __func__,
      Status
      ));
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  //
  // Locate 'bus-range'
  //
  Status = DtIo->GetU32 (DtIo, "bus-range", 0, &BusMin);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Can't get the min-bus number\n",
      __func__
      ));
  }

  Status = DtIo->GetU32 (DtIo, "bus-range", 1, &BusMax);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Can't get the max-bus number\n",
      __func__
      ));
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: ECAM region is [0x%Lx-0x%Lx]\n",
    __func__,
    EcamBase,
    EcamBase + (UINTN)Reg.Length - 1
    ));

  Attributes = EFI_PCI_ATTRIBUTE_ISA_IO_16 |
               EFI_PCI_ATTRIBUTE_ISA_MOTHERBOARD_IO |
               EFI_PCI_ATTRIBUTE_VGA_IO_16  |
               EFI_PCI_ATTRIBUTE_VGA_PALETTE_IO_16;

  AllocationAttributes = 0;
  if ((PMem.Base > PMem.Limit) &&
      (PMemAbove4G.Base > PMemAbove4G.Limit))
  {
    AllocationAttributes |= EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;
  }

  if ((MemAbove4G.Base <= MemAbove4G.Limit) ||
      (PMemAbove4G.Base <= PMemAbove4G.Limit))
  {
    AllocationAttributes |= EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
  }

  Status = DtIo->GetRange (DtIo, "dma-ranges", 0, &DmaRanges);
  //
  // Not handled yet, and we don't know the format. Assume TRUE
  // (valid for PC-like systems).
  //
  ASSERT (Status == EFI_NOT_FOUND);
  ASSERT (DtIo->IsDmaCoherent);
  Bridge->DmaAbove4G = TRUE;

  Bridge->Supports              = Attributes;
  Bridge->Attributes            = Attributes;
  Bridge->AllocationAttributes  = AllocationAttributes;
  Bridge->Bus.Base              = BusMin;
  Bridge->Bus.Limit             = BusMax;
  Bridge->NoExtendedConfigSpace = FALSE;

  CopyMem (&Bridge->Io, &Io, sizeof (Io));
  CopyMem (&Bridge->Mem, &Mem, sizeof (Mem));
  CopyMem (&Bridge->MemAbove4G, &MemAbove4G, sizeof (MemAbove4G));
  CopyMem (&Bridge->PMem, &PMem, sizeof (PMem));
  CopyMem (&Bridge->PMemAbove4G, &PMemAbove4G, sizeof (PMemAbove4G));

  return EFI_SUCCESS;
}

/**
  Populate and return all the root bridge instances in an array.

  @param  Count  Where to store the number of returned PCI_ROOT_BRIDGE structs.

  @return A Count-sized array of PCI_ROOT_BRIDGE on success. NULL otherwise.
**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  OUT UINTN  *Count
  )
{
  EFI_STATUS          Status;
  PCI_ROOT_BRIDGE     *Bridges;
  UINTN               Index;
  UINTN               BridgeIndex;
  EFI_HANDLE          *HandleBuffer;
  UINTN               HandleCount;
  EFI_DT_IO_PROTOCOL  *DtIo;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDtIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: LocateHandleBuffer: %r\n",
      __func__,
      Status
      ));
    return NULL;
  }

  for (Index = 0, BridgeIndex = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiDtIoProtocolGuid, (VOID **)&DtIo);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: HandleProtocol: %r\n",
        __func__,
        Status
        ));
      continue;
    }

    if (EFI_ERROR (DtIo->IsCompatible (DtIo, "pci-host-ecam-generic")) ||
        (DtIo->DeviceStatus != EFI_DT_STATUS_OKAY))
    {
      //
      // Not a supported node.
      //
      continue;
    }

    BridgeIndex++;
  }

  if (BridgeIndex == 0) {
    DEBUG ((DEBUG_INFO, "%a: No PCI host bridges present\n", __func__));
    FreePool (HandleBuffer);
    return NULL;
  }

  if (BridgeIndex > 1) {
    //
    // This boils down to there being a single PcdPciExpressBaseAddress. It should
    // still be possible to describe multiple RCs so long as they have non-overlapping
    // bus ranges (and thus share the same ECAM range), but there's nothing to test
    // with.
    //
    DEBUG ((
      DEBUG_INFO,
      "%a: Unsupported number of PCI host bridges present: %lu\n",
      __func__,
      BridgeIndex
      ));
    FreePool (HandleBuffer);
    return NULL;
  }

  Bridges = AllocateZeroPool (sizeof (PCI_ROOT_BRIDGE) * BridgeIndex);
  if (Bridges == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: %r\n", __func__, EFI_OUT_OF_RESOURCES));
    return NULL;
  }

  for (Index = 0, BridgeIndex = 0; Index < HandleCount; Index++) {
    MY_PCI_ROOT_BRIDGE_DEVICE_PATH  *DevicePath;
    //
    // Use BY_DRIVER instead of HandleProtocol to ensure
    // another driver can't reserve the device.
    //
    // Most of the handles here aren't "ours". Attempting
    // to open them may fail with EFI_ACCESS_DENIED if
    // they already have a driver attached.
    //
    Status = gBS->OpenProtocol (
                    HandleBuffer[Index],
                    &gEfiDtIoProtocolGuid,
                    (VOID **)&DtIo,
                    gImageHandle,
                    HandleBuffer[Index],
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        Status == EFI_ACCESS_DENIED ?
        DEBUG_VERBOSE : DEBUG_ERROR,
        "%a: HandleProtocol: %r\n",
        __func__,
        Status
        ));
      continue;
    }

    if (EFI_ERROR (DtIo->IsCompatible (DtIo, "pci-host-ecam-generic")) ||
        (DtIo->DeviceStatus != EFI_DT_STATUS_OKAY))
    {
      //
      // Don't forget to close unsupported handles, otherwise
      // other drivers won't be able to start!
      //
      Status = gBS->CloseProtocol (
                      HandleBuffer[Index],
                      &gEfiDtIoProtocolGuid,
                      gImageHandle,
                      HandleBuffer[Index]
                      );
      ASSERT_EFI_ERROR (Status);
      continue;
    }

    DevicePath = AllocateCopyPool (
                   sizeof mRootBridgeDevicePathTemplate,
                   &mRootBridgeDevicePathTemplate
                   );
    if (DevicePath == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: %r\n", __func__, EFI_OUT_OF_RESOURCES));
      break;
    }

    DevicePath->AcpiDevicePath.UID  = BridgeIndex;
    Bridges[BridgeIndex].Segment    = BridgeIndex;
    Bridges[BridgeIndex].DevicePath = (VOID *)DevicePath;

    Status = ProcessPciHost (DtIo, &Bridges[BridgeIndex]);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: ProcessPciHost[%lu]: %r\n",
        __func__,
        BridgeIndex,
        Status
        ));
      FreePool (DevicePath);
      break;
    }

    BridgeIndex++;
  }

  FreePool (HandleBuffer);
  *Count = BridgeIndex;

  return Bridges;
}

/**
  Free the root bridge instances array returned from PciHostBridgeGetRootBridges().

  @param Bridges The root bridge instances array.
  @param Count   The count of the array.
**/
VOID
EFIAPI
PciHostBridgeFreeRootBridges (
  PCI_ROOT_BRIDGE  *Bridges,
  UINTN            Count
  )
{
  UINTN  Index;

  for (Index = 0; Index < Count; Index++, Bridges++) {
    FreePool (Bridges->DevicePath);
  }
}

/**
  Inform the platform that a resource conflict happens.

  @param HostBridgeHandle Handle of the Host Bridge.
  @param Configuration    Pointer to PCI I/O and PCI memory resource
                          descriptors. The Configuration contains the resources
                          for all the root bridges. The resource for each root
                          bridge is terminated with END descriptor and an
                          additional END is appended indicating the end of the
                          entire resources. The resource descriptor field
                          values follow the description in
                          EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
                          .SubmitResources().
**/
VOID
EFIAPI
PciHostBridgeResourceConflict (
  EFI_HANDLE  HostBridgeHandle,
  VOID        *Configuration
  )
{
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Descriptor;
  UINTN                              RootBridgeIndex;

  DEBUG ((DEBUG_ERROR, "PciHostBridge: Resource conflict happens!\n"));

  RootBridgeIndex = 0;
  Descriptor      = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Configuration;
  while (Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    DEBUG ((DEBUG_ERROR, "RootBridge[%d]:\n", RootBridgeIndex++));
    for ( ; Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR; Descriptor++) {
      ASSERT (
        Descriptor->ResType <
        ARRAY_SIZE (mPciHostBridgeLibAcpiAddressSpaceTypeStr)
        );
      DEBUG ((
        DEBUG_ERROR,
        " %s: Length/Alignment = 0x%lx / 0x%lx\n",
        mPciHostBridgeLibAcpiAddressSpaceTypeStr[Descriptor->ResType],
        Descriptor->AddrLen,
        Descriptor->AddrRangeMax
        ));
      if (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {
        DEBUG ((
          DEBUG_ERROR,
          "     Granularity/SpecificFlag = %ld / %02x%s\n",
          Descriptor->AddrSpaceGranularity,
          Descriptor->SpecificFlag,
          ((Descriptor->SpecificFlag &
            EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE
            ) != 0) ? L" (Prefetchable)" : L""
          ));
      }
    }

    //
    // Skip the END descriptor for root bridge
    //
    ASSERT (Descriptor->Desc == ACPI_END_TAG_DESCRIPTOR);
    Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(
                                                       (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor + 1
                                                       );
  }
}
