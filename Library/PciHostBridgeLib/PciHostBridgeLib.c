/** @file
    PCI Host Bridge Library instance for a sample SOC

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci22.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <PiDxe.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/DtIo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DxeServicesTableLib.h>

#pragma pack(1)

typedef PACKED struct {
  ACPI_HID_DEVICE_PATH        AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    EndDevicePath;
} EFI_PCI_ROOT_BRIDGE_DEVICE_PATH;

#pragma pack ()

EFI_PCI_ROOT_BRIDGE_DEVICE_PATH  mEfiPciRootBridgeDevicePath[] = {
  {
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
  },
  {
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
      1
    },

    {
      END_DEVICE_PATH_TYPE,
      END_ENTIRE_DEVICE_PATH_SUBTYPE,
      {
        END_DEVICE_PATH_LENGTH,
        0
      }
    }
  },
};

GLOBAL_REMOVE_IF_UNREFERENCED
CHAR16  *mPciHostBridgeLibAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

UINTN  mHBCount = 0;

#define EFI_DT_PCI_HOST_RANGE_RELOCATABLE   BIT31
#define EFI_DT_PCI_HOST_RANGE_PREFETCHABLE  BIT30
#define EFI_DT_PCI_HOST_RANGE_ALIASED       BIT29
#define EFI_DT_PCI_HOST_RANGE_MMIO32        BIT25
#define EFI_DT_PCI_HOST_RANGE_IO            BIT24
#define EFI_DT_PCI_HOST_RANGE_MMIO64        (EFI_DT_PCI_HOST_RANGE_PREFETCHABLE \
  |EFI_DT_PCI_HOST_RANGE_MMIO32 | EFI_DT_PCI_HOST_RANGE_IO)
#define EFI_DT_PCI_HOST_RANGE_TYPEMASK      (EFI_DT_PCI_HOST_RANGE_PREFETCHABLE \
| EFI_DT_PCI_HOST_RANGE_ALIASED \
| EFI_DT_PCI_HOST_RANGE_MMIO32 | EFI_DT_PCI_HOST_RANGE_IO)

ACPI_HID_DEVICE_PATH  mRootBridgeDeviceNodeTemplate = {
  {
    ACPI_DEVICE_PATH,
    ACPI_DP,
    {
      (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
      (UINT8)((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
    }
  },
  EISA_PNP_ID (0x0A03),
  0
};

PCI_ROOT_BRIDGE  mRootBridgeTemplate[] = {
  {
    0,
    0,                                    // Supports;
    0,                                    // Attributes;
    FALSE,                                // DmaAbove4G;
    FALSE,                                // NoExtendedConfigSpace;
    FALSE,                                // ResourceAssigned;
    EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM |
    EFI_PCI_HOST_BRIDGE_MEM64_DECODE,     // AllocationAttributes
    { 0, 255 },                           // Bus
    { 0, 0   },                           // Io - to be fixed later
    { 0, 0   },                           // Mem - to be fixed later
    { 0, 0   },                           // MemAbove4G - to be fixed later
    { 0, 0   },                           // PMem - COMBINE_MEM_PMEM indicating no PMem and PMemAbove4GB
    { 0, 0   },                           // PMemAbove4G
    NULL                                  // DevicePath;
  }
};

/**
  Get the Bus range of the Pcie Bridge

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  ChildBase             The ChildBase of the 'bus-range' property

**/
EFI_DT_CELL
EFIAPI
DtIoGetSpaceCode (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_DT_BUS_ADDRESS  ChildBase
  )
{
  if (This->ChildAddressCells < 2) {
    return 0;
  }

  return (EFI_DT_CELL)(ChildBase >> ((This->ChildAddressCells - 1) * sizeof (EFI_DT_CELL) * 8));
}

EFI_STATUS
EFIAPI
MapGcdMmioSpace (
  IN    UINT64  Base,
  IN    UINT64  Size
  )
{
  EFI_STATUS  Status;

  Status = gDS->AddMemorySpace (
                  EfiGcdMemoryTypeMemoryMappedIo,
                  Base,
                  Size,
                  EFI_MEMORY_UC
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to add GCD memory space for region [0x%Lx+0x%Lx)\n",
      __FUNCTION__,
      Base,
      Size
      ));
    return Status;
  }

  Status = gDS->SetMemorySpaceAttributes (Base, Size, EFI_MEMORY_UC);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set memory space attributes for region [0x%Lx+0x%Lx)\n",
      __FUNCTION__,
      Base,
      Size
      ));
  }

  return Status;
}

EFI_STATUS
EFIAPI
ProcessPciHost (
  VOID
  )
{
  UINT64                    ConfigBase;
  UINT64                    ConfigSize;
  UINTN                     Len;
  UINTN                     Index;
  EFI_STATUS                Status;
  EFI_DT_IO_PROTOCOL        *DtIo;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  UINT64                    IoBase;
  UINT64                    IoSize;
  UINT64                    Mmio32Base;
  UINT64                    Mmio32Size;
  UINT64                    Mmio64Base;
  UINT64                    Mmio64Size;
  UINT64                    PreMemAbove4GBase;
  UINT64                    PreMemAbove4GSize;
  UINT32                    BusMin;
  UINT32                    BusMax;
  PCI_ROOT_BRIDGE_APERTURE  Io;
  PCI_ROOT_BRIDGE_APERTURE  Mem;
  PCI_ROOT_BRIDGE_APERTURE  MemAbove4G;
  PCI_ROOT_BRIDGE_APERTURE  PMem;
  PCI_ROOT_BRIDGE_APERTURE  PMemAbove4G;
  UINT64                    IoTranslation;
  UINT64                    Mmio32Translation;
  UINT64                    Mmio64Translation;
  EFI_DT_RANGE              Range;
  UINTN                     Index2;
  EFI_DT_RANGE              DmaRanges;
  EFI_DT_REG                Reg;
  EFI_DT_PROPERTY           Property;
  UINTN                     RangeGroups;
  EFI_DT_CELL               Spacecode;
  UINTN                     IndexOfReg;
  UINT64                    AllocationAttributes;

  Len               = 0;
  RangeGroups       = 0;
  IoBase            = 0;
  Mmio32Base        = 0;
  Mmio64Base        = MAX_UINT64;
  PreMemAbove4GBase = 0;
  BusMin            = 0;
  BusMax            = 0;
  IndexOfReg        = 0;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDtIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  ASSERT_EFI_ERROR (Status);

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiDtIoProtocolGuid, (VOID **)&DtIo);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: HandleProtocol: %r\n",
        __func__,
        Status
        ));
      continue;
    } else {
      if (!(DtIo->IsCompatible (DtIo, "pci-host-ecam-generic"))) {
        //
        // found the node or nodes by key words 'pci-host-ecam-generic'

        /*
        // The reference PCI DT node can be referred to as shown in the example below
          pci@2c000000 {
                  reg = <0x0 0x2C000000 0x0 0x1000000
                         0x9 0xc0000000 0x0 0x10000000>;
                  reg-names = "reg", "config";
                  ranges = <0x82000000  0x0 0x40000000  0x0 0x40000000 0x0 0x40000000>,
                           <0xc3000000  0x9 0x80000000  0x9 0x80000000 0x0 0x40000000>;
                  dma-coherent;
                  bus-range = <0x00 0xff>;
                  linux,pci-domain = <0x00>;
                  device_type = "pci";
                  compatible = "pci-host-ecam-generic";
                  #size-cells = <0x02>;
                  #address-cells = <0x03>;
          };

        */
        Status = DtIo->GetProp (DtIo, "ranges", &Property);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetProp:: Status=[%r]. \n",
            __func__,
            Status
            ));
          return Status;
        }

        RangeGroups = (Property.End - Property.Begin) /
                      (sizeof (EFI_DT_CELL) * (DtIo->ChildAddressCells + DtIo->AddressCells + DtIo->SizeCells));

        IoSize               = 0;
        Mmio32Size           = 0;
        Mmio64Size           = 0;
        PreMemAbove4GSize    = 0;
        IoTranslation        = 0;
        AllocationAttributes = 0;

        for (Index2 = 0; Index2 < RangeGroups; Index2++) {
          Status = DtIo->GetRange (DtIo, "ranges", Index2, &Range);
          if (EFI_ERROR (Status)) {
            DEBUG ((
              DEBUG_ERROR,
              "%a: DtIoGetRange is failed. \n",
              __func__
              ));
          }

          Spacecode = DtIoGetSpaceCode (DtIo, Range.ChildBase);
          switch (Spacecode & EFI_DT_PCI_HOST_RANGE_TYPEMASK) {
            case EFI_DT_PCI_HOST_RANGE_IO:
              IoBase        = Range.ChildBase;
              IoSize        = Range.Size;
              IoTranslation = Range.ParentBase - IoBase;
              break;

            case EFI_DT_PCI_HOST_RANGE_MMIO32:
              Mmio32Base        = Range.ChildBase;
              Mmio32Size        = Range.Size;
              Mmio32Translation = Range.ParentBase - Mmio32Base;

              if ((Mmio32Base > MAX_UINT32) || (Mmio32Size > MAX_UINT32) ||
                  (Mmio32Base + Mmio32Size > SIZE_4GB))
              {
                DEBUG ((
                  DEBUG_ERROR,
                  "%a: MMIO32 space invalid\n",
                  __FUNCTION__
                  ));
                break;
              }

              if (Mmio32Translation != 0) {
                DEBUG ((
                  DEBUG_ERROR,
                  "%a: unsupported nonzero MMIO32 translation "
                  "0x%Lx\n",
                  __FUNCTION__,
                  Mmio32Translation
                  ));
              }

              break;

            case EFI_DT_PCI_HOST_RANGE_MMIO64:
              if (mRootBridgeTemplate[mHBCount].AllocationAttributes &
                  EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM)
              {
                Mmio64Base        = Range.ChildBase;
                Mmio64Size        = Range.Size;
                Mmio64Translation = Range.ParentBase - Mmio64Base;

                if (Mmio64Translation != 0) {
                  DEBUG ((
                    DEBUG_ERROR,
                    "%a: unsupported nonzero MMIO64 translation "
                    "0x%Lx\n",
                    __FUNCTION__,
                    Mmio64Translation
                    ));
                }

                AllocationAttributes |= EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM +
                                        EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
              } else {
                PreMemAbove4GBase = Range.ChildBase;
                PreMemAbove4GSize = Range.Size;
                Mmio64Translation = Range.ParentBase - PreMemAbove4GBase;

                if (Mmio64Translation != 0) {
                  DEBUG ((
                    DEBUG_ERROR,
                    "%a: unsupported nonzero MMIO64 translation "
                    "0x%Lx\n",
                    __FUNCTION__,
                    Mmio64Translation
                    ));
                }

                AllocationAttributes |= EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
              }

              break;

            default:
              DEBUG ((
                DEBUG_ERROR,
                "%a: Unknow type is detected. \n",
                __func__
                ));
              break;
          }
        }

        if (Mmio32Size == 0) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: MMIO32 space empty\n",
            __FUNCTION__
            ));
          continue;
        }

        //
        // Locate 'config'
        //
        Status = DtIo->GetRegByName (DtIo, "config", &Reg);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetRegByName is failed. \n",
            __func__
            ));
        }

        //
        // Fetch the ECAM window.
        //
        ConfigBase = Reg.Base;
        ConfigSize = Reg.Length;
        DEBUG ((
          DEBUG_ERROR,
          "%a: GetReg:: ConfigBase=[%lx], ConfigSize=[%lx]. \n",
          __func__,
          ConfigBase,
          ConfigSize
          ));

        //
        // Locate 'bus-range'
        //
        Status = DtIo->GetU32 (DtIo, "bus-range", 0, &BusMin);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Can't get the min-bus number. \n",
            __func__
            ));
        }

        Status = DtIo->GetU32 (DtIo, "bus-range", 1, &BusMax);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Can't get the max-bus number. \n",
            __func__
            ));
        }

        DEBUG ((
          DEBUG_INFO,
          "%a: Config[0x%Lx+0x%Lx) Bus[0x%x..0x%x] "
          "Io[0x%Lx+0x%Lx)@0x%Lx Mem32[0x%Lx+0x%Lx)@0x0 Mem64[0x%Lx+0x%Lx)@0x0\n",
          __FUNCTION__,
          ConfigBase,
          ConfigSize,
          BusMin,
          BusMax,
          IoBase,
          IoSize,
          IoTranslation,
          Mmio32Base,
          Mmio32Size,
          Mmio64Base,
          Mmio64Size
          ));

        //
        // Map the ECAM space in the GCD memory map
        //
        Status = MapGcdMmioSpace (ConfigBase, ConfigSize);
        //
        // Comment it out here, but make sure to uncomment it in the production code
        // ASSERT_EFI_ERROR (Status);
        //

        if (IoSize != 0) {
          //
          // Map the MMIO window that provides I/O access - the PCI host bridge code
          // is not aware of this translation and so it will only map the I/O view
          // in the GCD I/O map.
          //
          Status = MapGcdMmioSpace (IoBase + IoTranslation, IoSize);
          //
          // Comment it out here, but make sure to uncomment it in the production code
          // ASSERT_EFI_ERROR (Status);
          //
        }

        ZeroMem (&Io, sizeof (Io));
        ZeroMem (&Mem, sizeof (Mem));
        ZeroMem (&MemAbove4G, sizeof (MemAbove4G));
        ZeroMem (&PMem, sizeof (PMem));
        ZeroMem (&PMemAbove4G, sizeof (PMemAbove4G));

        if (IoSize != 0) {
          Io.Base  = IoBase;
          Io.Limit = IoBase + IoSize - 1;
        } else {
          Io.Base  = 1;
          Io.Limit = 0;
        }

        Mem.Base  = Mmio32Base;
        Mem.Limit = Mmio32Base + Mmio32Size - 1;

        if (Mmio64Size != 0) {
          MemAbove4G.Base  = Mmio64Base;
          MemAbove4G.Limit = Mmio64Base + Mmio64Size - 1;
        } else {
          MemAbove4G.Base  = MAX_UINT64;
          MemAbove4G.Limit = 0;
        }

        //
        // No separate ranges for prefetchable and non-prefetchable BARs
        //
        PMem.Base  = MAX_UINT64;
        PMem.Limit = 0;

        if (PreMemAbove4GSize != 0) {
          PMemAbove4G.Base  = PreMemAbove4GBase;
          PMemAbove4G.Limit = PreMemAbove4GBase + PreMemAbove4GSize - 1;
        } else {
          PMemAbove4G.Base  = MAX_UINT64;
          PMemAbove4G.Limit = 0;
        }

        Status = DtIo->GetRange (DtIo, "dma-ranges", 0, &DmaRanges);
        if (!EFI_ERROR (Status)) {
          //
          // here is just the sample code when there is a single dma-ranges.
          // Sometimes there is a dma-ranges group, need to check the scope
          // one by one. (need to improve it later)
          //
          Spacecode = DtIoGetSpaceCode (DtIo, DmaRanges.ChildBase);
          if (Spacecode & EFI_DT_PCI_HOST_RANGE_MMIO64) {
            mRootBridgeTemplate[mHBCount].DmaAbove4G = TRUE;
          }
        } else if (Status == EFI_NOT_FOUND) {
          mRootBridgeTemplate[mHBCount].DmaAbove4G = PcdGetBool (PcdDmaAbove4G);
        }

        mRootBridgeTemplate[mHBCount].Supports              = PcdGet64 (PcdSupportedAttributes);
        mRootBridgeTemplate[mHBCount].Attributes            = PcdGet64 (PcdInitialAttributes);
        mRootBridgeTemplate[mHBCount].AllocationAttributes  = AllocationAttributes;
        mRootBridgeTemplate[mHBCount].Bus.Base              = BusMin;
        mRootBridgeTemplate[mHBCount].Bus.Limit             = BusMax;
        mRootBridgeTemplate[mHBCount].NoExtendedConfigSpace = PcdGetBool (PcdNoExtendedConfigSpace);
        mRootBridgeTemplate[mHBCount].ResourceAssigned      = PcdGetBool (PcdResourceAssigned);

        CopyMem (&mRootBridgeTemplate[mHBCount].Io, &Io, sizeof (Io));
        CopyMem (&mRootBridgeTemplate[mHBCount].Mem, &Mem, sizeof (Mem));
        CopyMem (&mRootBridgeTemplate[mHBCount].MemAbove4G, &MemAbove4G, sizeof (MemAbove4G));
        CopyMem (&mRootBridgeTemplate[mHBCount].PMem, &PMem, sizeof (PMem));
        CopyMem (&mRootBridgeTemplate[mHBCount].PMemAbove4G, &PMemAbove4G, sizeof (PMemAbove4G));

        mHBCount++;
      }
    }
  }

  return Status;
}

/**
  Return all the root bridge instances in an array.

  @param Count  Return the count of root bridge instances.

  @return All the root bridge instances in an array.
          The array should be passed into PciHostBridgeFreeRootBridges()
          when it's not used.
**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  OUT UINTN  *Count
  )
{
  EFI_STATUS       Status;
  PCI_ROOT_BRIDGE  *Bridges;
  UINTN            Index;

  *Count = 0;

  Status = ProcessPciHost ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to discover PCI host bridge: %r\n",
      __FUNCTION__,
      Status
      ));
    return NULL;
  }

  if (mHBCount == 0) {
    DEBUG ((DEBUG_INFO, "%a: PCI host bridge not present\n", __FUNCTION__));
    return NULL;
  }

  Bridges = AllocatePool (sizeof (PCI_ROOT_BRIDGE) * mHBCount);
  if (Bridges == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: %r\n", __FUNCTION__, EFI_OUT_OF_RESOURCES));
    return NULL;
  }

  for (Index = 0; Index < mHBCount; Index++) {
    mEfiPciRootBridgeDevicePath[Index].AcpiDevicePath.UID = Index;
    mRootBridgeTemplate[Index].Segment                    = Index;
    mRootBridgeTemplate[Index].DevicePath                 = AppendDevicePathNode (NULL, &mEfiPciRootBridgeDevicePath[Index].AcpiDevicePath.Header);

    CopyMem (Bridges + Index, &mRootBridgeTemplate[Index], sizeof (PCI_ROOT_BRIDGE));
  }

  *Count = mHBCount;

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
  FreePool (Bridges->DevicePath);
}

/**
  Inform the platform that the resource conflict happens.

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
