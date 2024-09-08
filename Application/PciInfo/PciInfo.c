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
#include <Library/HandleParsingLib.h>
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
  Print (L"Usage: %s seg bus dev func\n", Name);
  Print (L"       %s handle\n", Name);
  Print (L"       %s [-v]\n", Name);
  return EFI_INVALID_PARAMETER;
}

STATIC
VOID
DumpAttrs (
  IN UINT64  Attrs
  )
{
  UINTN        Index;
  CONST CHAR8  *Str;

  for (Index = 0; Attrs != 0; Attrs >>= 1, Index++) {
    if ((Attrs & 1) == 0) {
      continue;
    }

    switch (1UL << Index) {
      case EFI_PCI_IO_ATTRIBUTE_ISA_MOTHERBOARD_IO:
        Str = "ISA_MB";
        break;
      case EFI_PCI_IO_ATTRIBUTE_ISA_IO:
        Str = "ISA";
        break;
      case EFI_PCI_IO_ATTRIBUTE_VGA_PALETTE_IO:
        Str = "PLT";
        break;
      case EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY:
        Str = "VGA_MEM";
        break;
      case EFI_PCI_IO_ATTRIBUTE_VGA_IO:
        Str = "VGA";
        break;
      case EFI_PCI_IO_ATTRIBUTE_IDE_PRIMARY_IO:
        Str = "IDE1";
        break;
      case EFI_PCI_IO_ATTRIBUTE_IDE_SECONDARY_IO:
        Str = "IDE2";
        break;
      case EFI_PCI_IO_ATTRIBUTE_MEMORY_WRITE_COMBINE:
        Str = "WC";
        break;
      case EFI_PCI_IO_ATTRIBUTE_IO:
        Str = "IO";
        break;
      case EFI_PCI_IO_ATTRIBUTE_MEMORY:
        Str = "MEM";
        break;
      case EFI_PCI_IO_ATTRIBUTE_BUS_MASTER:
        Str = "BM";
        break;
      case EFI_PCI_IO_ATTRIBUTE_MEMORY_CACHED:
        Str = "MC";
        break;
      case EFI_PCI_IO_ATTRIBUTE_MEMORY_DISABLE:
        Str = "MD";
        break;
      case EFI_PCI_IO_ATTRIBUTE_EMBEDDED_DEVICE:
        Str = "ED";
        break;
      case EFI_PCI_IO_ATTRIBUTE_EMBEDDED_ROM:
        Str = "ER";
        break;
      case EFI_PCI_IO_ATTRIBUTE_DUAL_ADDRESS_CYCLE:
        Str = "DAC";
        break;
      case EFI_PCI_IO_ATTRIBUTE_ISA_IO_16:
        Str = "ISA16";
        break;
      case EFI_PCI_IO_ATTRIBUTE_VGA_PALETTE_IO_16:
        Str = "PLT16";
        break;
      case EFI_PCI_IO_ATTRIBUTE_VGA_IO_16:
        Str = "VGA16";
        break;
      default:
        Str = "??";
    }

    Print (L"%a ", Str);
  }

  Print (L"\n");
}

STATIC
VOID
ParseImage (
  IN VOID                *RomImage,
  IN VOID                *RomHeader,
  IN UINTN               Length,
  IN PCI_DATA_STRUCTURE  *Pcir
  )
{
  CHAR16                        *Type;
  UINTN                         RomOffset;
  UINTN                         InitializationSize;
  BOOLEAN                       Supported;
  EFI_PCI_EXPANSION_ROM_HEADER  *EfiRomHeader;

  RomOffset = (UINTN)RomHeader - (UINTN)RomImage;
  if (Pcir->CodeType == PCI_CODE_TYPE_EFI_IMAGE) {
    Type = L"UEFI";
  } else if (Pcir->CodeType == PCI_CODE_TYPE_PCAT_IMAGE) {
    Type = L"BIOS";
  } else if (Pcir->CodeType == 1) {
    Type = L"1275";
  } else if (Pcir->CodeType == 2) {
    Type = L"HPPA";
  } else {
    Type = L"????";
  }

  Supported = FALSE;
  if (Pcir->CodeType != PCI_CODE_TYPE_EFI_IMAGE) {
    Print (
      L"+0x%08x: %s (0x%04x) image (0x%x bytes)\n",
      RomOffset,
      Type,
      Pcir->CodeType,
      Length
      );
  } else {
    EfiRomHeader       = (VOID *)RomHeader;
    InitializationSize = EfiRomHeader->InitializationSize * 512;

    Print (
      L"+0x%08x: %s (0x%04x) image (0x%x bytes)\n",
      RomOffset,
      Type,
      EfiRomHeader->EfiMachineType,
      Length
      );

    Print (
      L"+0x%08x:\tSubsystem: 0x%x\n",
      RomOffset,
      EfiRomHeader->EfiSubsystem
      );
    Print (
      L"+0x%08x:\tInitializationSize: 0x%x (bytes)\n",
      RomOffset,
      InitializationSize
      );
    Print (
      L"+0x%08x:\tEfiImageHeaderOffset: 0x%x\n",
      RomOffset,
      EfiRomHeader->EfiImageHeaderOffset
      );
    Print (
      L"+0x%08x:\tCompressed: %s\n",
      RomOffset,
      EfiRomHeader->CompressionType == 0 ? L"no" : L"yes"
      );

    if ((Length < InitializationSize) ||
        (EfiRomHeader->EfiImageHeaderOffset > InitializationSize))
    {
      Print (L"+0x%08x: Image is CORRUPT and UNSUPPORTED\n", RomOffset);
    } else {
      Supported = TRUE;
    }
  }
}

STATIC
VOID
ParseImages (
  IN VOID    *RomImage,
  IN UINT64  RomSize
  )
{
  PCI_EXPANSION_ROM_HEADER  *RomHeader;
  PCI_DATA_STRUCTURE        *RomPcir;
  UINT8                     Indicator;

  Indicator = 0;
  RomHeader = RomImage;
  if (RomHeader == NULL) {
    return;
  }

  do {
    UINTN  ImageLength;

    if (RomHeader->Signature != PCI_EXPANSION_ROM_HEADER_SIGNATURE) {
      RomHeader = (PCI_EXPANSION_ROM_HEADER *)((UINT8 *)RomHeader + 512);
      continue;
    }

    //
    // The PCI Data Structure must be DWORD aligned.
    //
    if ((RomHeader->PcirOffset == 0) ||
        ((RomHeader->PcirOffset & 3) != 0) ||
        ((UINT8 *)RomHeader + RomHeader->PcirOffset + sizeof (PCI_DATA_STRUCTURE) > (UINT8 *)RomImage + RomSize))
    {
      break;
    }

    RomPcir = (PCI_DATA_STRUCTURE *)((UINT8 *)RomHeader + RomHeader->PcirOffset);
    if (RomPcir->Signature != PCI_DATA_STRUCTURE_SIGNATURE) {
      break;
    }

    ImageLength = RomPcir->ImageLength;
    if (RomPcir->CodeType == PCI_CODE_TYPE_PCAT_IMAGE) {
      EFI_LEGACY_EXPANSION_ROM_HEADER  *Legacy = (void *)RomHeader;
      //
      // Some legacy cards do not report the correct ImageLength so use
      // the maximum of the legacy length and the PCIR Image Length.
      //
      ImageLength = MAX (ImageLength, Legacy->Size512);
    }

    ParseImage (RomImage, RomHeader, ImageLength * 512, RomPcir);
    Indicator = RomPcir->Indicator;
    RomHeader = (PCI_EXPANSION_ROM_HEADER *)
                ((UINT8 *)RomHeader + ImageLength * 512);
  } while (((UINT8 *)RomHeader < (UINT8 *)RomImage + RomSize) &&
           ((Indicator & 0x80) == 0x00));
}

STATIC
EFI_STATUS
Dump (
  IN EFI_HANDLE           Handle,
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
  UINT64                             Attributes;

  Hdr.DeviceId = Hdr.VendorId = 0xffff;
  PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16, 0, sizeof (Hdr) / sizeof (UINT16), &Hdr);
  if (!mVerbose) {
    Print (
      L"[%x] %04x:%02x:%02x.%02x: Vendor: %04x Device: %04x\n",
      ConvertHandleToHandleIndex (Handle),
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
    L"[%x] %04x:%02x:%02x.%02x info:\n",
    ConvertHandleToHandleIndex (Handle),
    Seg,
    Bus,
    Dev,
    Func
    );

  Print (L"-------------------------\n");

  Print (L"     Vendor: %04x Device: %04x\n", Hdr.VendorId, Hdr.DeviceId);
  Status = PciIo->Attributes (PciIo, EfiPciIoAttributeOperationSupported, 0, &Attributes);
  if (!EFI_ERROR (Status)) {
    Print (L"  Supported: ");
    DumpAttrs (Attributes);
  }

  Status = PciIo->Attributes (PciIo, EfiPciIoAttributeOperationGet, 0, &Attributes);
  if (!EFI_ERROR (Status)) {
    Print (L"    Current: ");
    DumpAttrs (Attributes);
  }

  if (PciIo->RomSize != 0) {
    Print (L"       ROMs:\n");
    ParseImages (PciIo->RomImage, PciIo->RomSize);
  }

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

    Print (L"       BAR%u: ", Index);

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
  } else if ((Argc - GetOptContext.OptIndex) == 1) {
    EFI_HANDLE  Handle;
    UINTN       Arg;

    mVerbose = TRUE;
    Arg      = StrHexToUintn (Argv[GetOptContext.OptIndex]);
    Handle   = ConvertHandleIndexToHandle (Arg);
    if (Handle == NULL) {
      Handle = (EFI_HANDLE)Arg;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiPciIoProtocolGuid,
                    (VOID *)&PciIo
                    );

    if (Status != EFI_SUCCESS) {
      Print (L"Couldn't get EFI_PCI_IO_PROTOCOL: %r\n", Status);
      return Status;
    }

    Status = PciIo->GetLocation (PciIo, &WantSeg, &WantBus, &WantDev, &WantFunc);
    if (Status != EFI_SUCCESS) {
      Print (L"GetLocation failed: %r\n", Status);
      return Status;
    }

    return Dump (
             Handle,
             WantSeg,
             WantBus,
             WantDev,
             WantFunc,
             PciIo
             );
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
               PciHandles[PciIndex],
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
