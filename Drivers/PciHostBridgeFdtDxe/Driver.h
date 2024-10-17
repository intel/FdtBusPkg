/** @file
    DT-based PCI(e) host bridge driver.

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <Uefi.h>
#include <PiDxe.h>
#include <IndustryStandard/Pci22.h>
#include <IndustryStandard/PciExpress21.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FbpUtilsLib.h>
#include <Library/FbpPciUtilsLib.h>
#include <Library/PcdLib.h>
#include <Protocol/DtIo.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>

extern EFI_COMPONENT_NAME_PROTOCOL   gComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gComponentName2;
extern EFI_DRIVER_BINDING_PROTOCOL   gDriverBinding;

//
// Macros to translate device address to host address and vice versa. According
// to UEFI 2.7, device address = host address + translation offset.
//
#define TO_HOST_ADDRESS(DeviceAddress, TranslationOffset)  ((DeviceAddress) - (TranslationOffset))
#define TO_DEVICE_ADDRESS(HostAddress, TranslationOffset)  ((HostAddress) + (TranslationOffset))

//
// According to UEFI 2.7, Device Address = Host Address + Translation,
// so Translation = Device Address - Host Address.
// On platforms where Translation is not zero, the subtraction is probably to
// be performed with UINT64 wrap-around semantics, for we may translate an
// above-4G host address into a below-4G device address for legacy PCIe device
// compatibility.
//
// NOTE: The alignment of Translation is required to be larger than any BAR
// alignment in the same root bridge, so that the same alignment can be
// applied to both device address and host address, which simplifies the
// situation and makes the current resource allocation code in generic PCI
// host bridge driver still work.
//
#define RT(Range)           ((UINT64)((Range).ChildBase - (Range).TranslatedParentBase))
#define RB(Range)           ((UINT64)((Range).ChildBase))
#define RS(Range)           ((UINT64)((Range).Length))
#define RL(Range)           ((UINT64)((Range).ChildBase + (Range).Length - 1))
#define RANGE_VALID(Range)  ((Range).Length != 0)

#define PCI_ROOT_BRIDGE_SIGNATURE  SIGNATURE_32 ('d', 't', 'r', 'b')

#define ROOT_BRIDGE_FROM_THIS(a)  CR (a, PCI_ROOT_BRIDGE_INSTANCE, RootBridgeIo, PCI_ROOT_BRIDGE_SIGNATURE)

#define PCI_RESOURCE_LESS  0xFFFFFFFFFFFFFFFEULL

typedef enum {
  TypeIo = 0,
  TypeMem32,
  TypePMem32,
  TypeMem64,
  TypePMem64,
  TypeBus,
  TypeMax
} PCI_RESOURCE_TYPE;

typedef enum {
  ResNone,
  ResSubmitted,
  ResAllocated,
  ResStatusMax
} RES_STATUS;

typedef struct {
  EFI_PHYSICAL_ADDRESS    Base;
  UINT64                  Length;
  UINT64                  Alignment;
  RES_STATUS              Status;
  BOOLEAN                 ResTracked;
} PCI_RES_NODE;

typedef struct _PCI_ROOT_BRIDGE_INSTANCE PCI_ROOT_BRIDGE_INSTANCE;

struct _PCI_ROOT_BRIDGE_INSTANCE {
  UINT32                                              Signature;
  EFI_HANDLE                                          Controller;
  EFI_DT_IO_PROTOCOL                                  *DtIo;
  CHAR16                                              *DevicePathStr;
  VOID                                                *ConfigBuffer;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL                     RootBridgeIo;

  UINT32                                              Segment;
  EFI_DT_REG                                          ConfigReg;
  UINT64                                              Attributes;
  UINT64                                              Supports;
  PCI_RES_NODE                                        ResAllocNode[TypeMax];
  EFI_DT_RANGE                                        BusRange;
  EFI_DT_RANGE                                        IoRange;
  EFI_DT_RANGE                                        MemRange;
  EFI_DT_RANGE                                        PMemRange;
  EFI_DT_RANGE                                        MemAbove4GRange;
  EFI_DT_RANGE                                        PMemAbove4GRange;
  EFI_DT_RANGE                                        VgaMemRange;
  EFI_DT_RANGE                                        VgaIo1Range;
  EFI_DT_RANGE                                        VgaIo2Range;
  BOOLEAN                                             DmaAbove4G;
  BOOLEAN                                             NoExtendedConfigSpace;
  BOOLEAN                                             KeepExistingConfig;
  //
  // Manipulated by HostBridge.c.
  //
  BOOLEAN                                             ResourceSubmitted;
  BOOLEAN                                             CanRestart;
  UINT64                                              AllocationAttributes;
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL    ResAlloc;
};

#define PCI_ROOT_BRIDGE_FROM_THIS(a)  CR (a, PCI_ROOT_BRIDGE_INSTANCE, RootBridgeIo, PCI_ROOT_BRIDGE_SIGNATURE)

#define PCI_ROOT_BRIDGE_FROM_RES_ALLOC(a)  CR (a, PCI_ROOT_BRIDGE_INSTANCE, ResAlloc, PCI_ROOT_BRIDGE_SIGNATURE)

VOID
HostBridgeInit (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge
  );

EFI_STATUS
HostBridgeKeepExistingConfig (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge
  );

EFI_STATUS
HostBridgeFreeExistingConfig (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge
  );

EFI_STATUS
RootBridgeCreate (
  IN  EFI_DT_IO_PROTOCOL        *DtIo,
  IN  EFI_HANDLE                Controller,
  IN  EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
  OUT PCI_ROOT_BRIDGE_INSTANCE  **RootBridge
  );

VOID
RootBridgeFree (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge
  );

UINT64
GetTranslationByResourceType (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge,
  IN  PCI_RESOURCE_TYPE         ResourceType
  );

#endif /* __DRIVER_H__ */
