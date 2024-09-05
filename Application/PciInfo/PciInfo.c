/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
    Copyright (C) 2016 Andrei Evgenievich Warkentin

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/DevicePathLib.h>
#include <Library/FbpAppUtilsLib.h>
#include <Protocol/PciIo.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Acpi.h>

STATIC BOOLEAN  mVerbose;

STATIC
EFI_STATUS
Usage (
  IN CHAR16  *Name
  )
{
  Print (L"Usage: %s [-v] [seg bus dev func]\n", Name);
  return EFI_INVALID_PARAMETER;
}

STATIC
EFI_STATUS
Dump (
  IN UINTN                Seg,
  IN UINTN                Bus,
  IN UINTN                Dev,
  IN UINTN                Func,
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  UINTN                              Index;
  EFI_STATUS                         Status;
  PCI_DEVICE_INDEPENDENT_REGION      Hdr;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *BarDesc;

  Hdr.DeviceId = Hdr.VendorId = 0xffff;
  PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, 0, sizeof (Hdr) / sizeof (UINT16), &Hdr);
  if (!mVerbose) {
    Print (
      L"%04x:%02x:%02x.%02x: Vendor: %04x Device: %04x\n",
      Seg,
      Bus,
      Dev,
      Func,
      Hdr.VendorId,
      Hdr.DeviceId
      );
    return EFI_SUCCESS;
  }

  Print (
    L"%04x:%02x:%02x.%02x info:\n",
    Seg,
    Bus,
    Dev,
    Func
    );

  Print (L"-------------------------\n");

  Print (L"Vendor: %04x Device: %04x\n", Hdr.VendorId, Hdr.DeviceId);

  for (Index = 0; Index < PCI_MAX_BAR; Index++) {
    BarDesc = NULL;
    Status  = PciIo->GetBarAttributes (
                       PciIo,
                       Index,
                       NULL,
                       (VOID **)&BarDesc
                       );
    if (Status == EFI_UNSUPPORTED) {
      break;
    }

    Print (L"BAR%u: ", Index);

    if (EFI_ERROR (Status)) {
      Print (L"error fetching (%r)\n", Status);
      continue;
    }

    if (BarDesc->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {
      Print (L"MEM%u ", BarDesc->AddrSpaceGranularity);
    } else if (BarDesc->ResType == ACPI_ADDRESS_SPACE_TYPE_IO) {
      Print (L"IO    ");
    } else {
      Print (L"bad type 0x%x\n", BarDesc->ResType);
      continue;
    }

    Print (
      L"CPU 0x%016lx -> PCI 0x%016lx (0x%x)\n",
      BarDesc->AddrRangeMin,
      BarDesc->AddrRangeMin + BarDesc->AddrTranslationOffset,
      BarDesc->AddrLen
      );

    FreePool (BarDesc);
  }

  Print (L"\n");

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EntryPoint (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN                Argc;
  CHAR16               **Argv;
  UINTN                WantSeg;
  UINTN                WantBus;
  UINTN                WantDev;
  UINTN                WantFunc;
  UINTN                PciIndex;
  UINTN                PciCount;
  EFI_STATUS           Status;
  EFI_HANDLE           *PciHandles;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  GET_OPT_CONTEXT      GetOptContext;
  BOOLEAN              AllDevs;

  Status = GetShellArgcArgv (ImageHandle, &Argc, &Argv);
  if (Status != EFI_SUCCESS) {
    Print (
      L"This program requires Microsoft Windows.\n"
      "Just kidding...only the UEFI Shell!\n"
      );
    return EFI_ABORTED;
  }

  INIT_GET_OPT_CONTEXT (&GetOptContext);
  while ((Status = GetOpt (
                     Argc,
                     Argv,
                     L"",
                     &GetOptContext
                     )) == EFI_SUCCESS)
  {
    switch (GetOptContext.Opt) {
      case L'v':
        mVerbose = TRUE;
        break;
      default:
        Print (L"Unknown option '%c'\n", GetOptContext.Opt);
        return Usage (Argv[0]);
    }
  }

  if ((Argc - GetOptContext.OptIndex) == 0) {
    WantSeg  = (UINTN)-1;
    WantBus  = (UINTN)-1;
    WantDev  = (UINTN)-1;
    WantFunc = (UINTN)-1;
    AllDevs  = TRUE;
  } else if ((Argc - GetOptContext.OptIndex) == 4) {
    WantSeg  = StrHexToUintn (Argv[GetOptContext.OptIndex]);
    WantBus  = StrHexToUintn (Argv[GetOptContext.OptIndex + 1]);
    WantDev  = StrHexToUintn (Argv[GetOptContext.OptIndex + 2]);
    WantFunc = StrHexToUintn (Argv[GetOptContext.OptIndex + 3]);
    AllDevs  = FALSE;
    mVerbose = TRUE;
  } else {
    return Usage (Argv[0]);
  }

  PciCount   = 0;
  PciHandles = NULL;
  Status     = gBS->LocateHandleBuffer (
                      ByProtocol,
                      &gEfiPciIoProtocolGuid,
                      NULL,
                      &PciCount,
                      &PciHandles
                      );
  if (Status != EFI_SUCCESS) {
    Print (L"No PCI devices found\n");
    return EFI_SUCCESS;
  }

  for (PciIndex = 0; PciIndex < PciCount; PciIndex++) {
    UINTN  Seg;
    UINTN  Bus;
    UINTN  Dev;
    UINTN  Func;

    Status = gBS->HandleProtocol (
                    PciHandles[PciIndex],
                    &gEfiPciIoProtocolGuid,
                    (VOID *)&PciIo
                    );

    if (Status != EFI_SUCCESS) {
      Print (L"Couldn't get EFI_PCI_IO_PROTOCOL: %r\n", Status);
      continue;
    }

    Status = PciIo->GetLocation (PciIo, &Seg, &Bus, &Dev, &Func);
    if (Status != EFI_SUCCESS) {
      Print (L"GetLocation failed: %r\n", Status);
      continue;
    }

    if (!AllDevs) {
      if ((WantSeg != Seg) ||
          (WantBus != Bus) ||
          (WantDev != Dev) ||
          (WantFunc != Func))
      {
        continue;
      }
    }

    Status = Dump (
               Seg,
               Bus,
               Dev,
               Func,
               PciIo
               );
    if (!AllDevs) {
      break;
    }
  }

  if (!AllDevs) {
    if (PciIndex == PciCount) {
      Print (
        L"%04x:%02x:%02x.%02x not found\n",
        WantSeg,
        WantBus,
        WantDev,
        WantFunc
        );
      Status = EFI_NOT_FOUND;
    }
  }

  gBS->FreePool (PciHandles);
  return Status;
}
