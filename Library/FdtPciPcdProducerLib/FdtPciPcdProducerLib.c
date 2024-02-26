/** @file
    DT I/O-based library for consumers of PCI related dynamic PCDs.

    Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/DtIo.h>
#include <Library/FbpUtilsLib.h>
#include <Library/FbpPciUtilsLib.h>

STATIC VOID       *mDtIoRegistration;
STATIC EFI_EVENT  mDtIoEvent;

/**
  Process the DT I/O device handles compatible to pci-host-ecam-generic.

  @param  Handle                EFI_HANDLE with DT I/O protocol installed.

  @return EFI_STATUS            EFI_SUCCESS if a compatible handle is processed.
**/
STATIC
EFI_STATUS
ProcessHandle (
  IN EFI_HANDLE  Handle
  )
{
  EFI_DT_REG            Reg;
  EFI_DT_RANGE          Range;
  EFI_DT_IO_PROTOCOL    *DtIo;
  EFI_STATUS            Status;
  BOOLEAN               FoundIoTranslation;
  EFI_PHYSICAL_ADDRESS  RegBase;
  UINTN                 Index;

  //
  // Use HandleProtocol here. The main user of the device is
  // PciHostBridgeLib/PciHostBridgeDxe, which will open BY_DRIVER.
  //
  Status = gBS->HandleProtocol (Handle, &gEfiDtIoProtocolGuid, (VOID **)&DtIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: HandleProtocol: %r\n",
      __func__,
      Status
      ));
    return Status;
  }

  Status = DtIo->IsCompatible (DtIo, "pci-host-ecam-generic");
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (DtIo->DeviceStatus != EFI_DT_STATUS_OKAY) {
    return EFI_UNSUPPORTED;
  }

  //
  // Locate 'config'.
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
      "%a: couldn't find ECAM window\n",
      __func__
      ));
    return Status;
  }

  Status = FbpRegToPhysicalAddress (&Reg, &RegBase);
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

  Status = PcdSet64S (PcdPciExpressBaseAddress, RegBase);
  ASSERT_EFI_ERROR (Status);

  Status = PcdSetBoolS (PcdPciDisableBusEnumeration, FALSE);
  ASSERT_EFI_ERROR (Status);

  for (Index = 0, FoundIoTranslation = FALSE,
       Status = DtIo->GetRange (DtIo, "ranges", Index, &Range);
       !FoundIoTranslation && !EFI_ERROR (Status);
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
        if (((UINT32)Range.ChildBase <= MAX_UINT32) &&
            ((UINT32)(Range.ChildBase + Range.Length - 1) <= MAX_UINT32))
        {
          Status = PcdSet64S (PcdPciIoTranslation, RangeCpuBase - Range.ChildBase);
          ASSERT_EFI_ERROR (Status);
          FoundIoTranslation = TRUE;
        }

        break;
      default:
        continue;
    }
  }

  if (!FoundIoTranslation) {
    //
    // Support for I/O BARs is not mandatory, and so it does not make sense
    // to abort in the general case. So leave it up to the actual driver to
    // complain about this if it wants to, and just issue a warning here.
    //
    DEBUG ((
      DEBUG_WARN,
      "%a: couldn't find I/O translation\n",
      __func__
      ));
  }

  return EFI_SUCCESS;
}

/**
  Callback on DT I/O protocol installation.

  @param  Event                 EFI_EVENT.
  @param  Context               Not used.
**/
STATIC
VOID
EFIAPI
OnDtIoInstall (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN       HandleSize;
  EFI_HANDLE  Handle;
  EFI_STATUS  Status;

  if (PcdGet64 (PcdPciExpressBaseAddress) != MAX_UINT64) {
    //
    // Assume that the fact that PciExpressBaseAddress has been changed from
    // its default value of MAX_UINT64 implies that this code has been
    // executed already, in the context of another module. That means we can
    // assume that PcdPciIoTranslation has been discovered from the DT node
    // as well.
    //
    gBS->CloseEvent (Event);
    return;
  }

  while (TRUE) {
    HandleSize = sizeof (EFI_HANDLE);
    Status     = gBS->LocateHandle (
                        ByRegisterNotify,
                        NULL,
                        mDtIoRegistration,
                        &HandleSize,
                        &Handle
                        );
    if (Status == EFI_NOT_FOUND) {
      break;
    }

    ASSERT_EFI_ERROR (Status);

    if (!EFI_ERROR (ProcessHandle (Handle))) {
      //
      // No more event callbacks as we have everything we need.
      //
      gBS->CloseEvent (Event);
      return;
    }
  }
}

RETURN_STATUS
EFIAPI
FdtPciPcdProducerLibDestructor (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;
  if (mDtIoEvent != NULL) {
    Status = gBS->CloseEvent (mDtIoEvent);
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}

RETURN_STATUS
EFIAPI
FdtPciPcdProducerLibConstructor (
  VOID
  )
{
  UINTN       Index;
  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *HandleBuffer;

  if (PcdGet64 (PcdPciExpressBaseAddress) != MAX_UINT64) {
    //
    // Assume that the fact that PciExpressBaseAddress has been changed from
    // its default value of MAX_UINT64 implies that this code has been
    // executed already, in the context of another module. That means we can
    // assume that PcdPciIoTranslation has been discovered from the DT node
    // as well.
    //
    return EFI_SUCCESS;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDtIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_NOT_FOUND) {
      //
      // Loaded before FdtBusDxe ran. Set a protocol notification handler.
      //
      Status = gBS->CreateEvent (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      OnDtIoInstall,
                      NULL,
                      &mDtIoEvent
                      );
      ASSERT_EFI_ERROR (Status);

      Status = gBS->RegisterProtocolNotify (
                      &gEfiDtIoProtocolGuid,
                      mDtIoEvent,
                      &mDtIoRegistration
                      );
      ASSERT_EFI_ERROR (Status);
    } else {
      DEBUG ((
        DEBUG_ERROR,
        "%a: LocateHandleBuffer: %r\n",
        __func__,
        Status
        ));
    }

    return Status;
  }

  //
  // Assume that if LocateHandleBuffer succeeded, the set
  // of devices initially scannned by FdtBusDxe includes
  // the PCIe RCs. If they don't, then the PCIe RCs
  // depend on some other driver and you would have other
  // issues.
  //

  for (Index = 0; Index < HandleCount; Index++) {
    Status = ProcessHandle (HandleBuffer[Index]);
    if (!EFI_ERROR (Status)) {
      //
      // Grab the first one. Multiple PCIe nodes not supported by
      // PcdPciExpressBaseAddress and PcdPciIoTranslation.
      //
      break;
    }
  }

  gBS->FreePool (HandleBuffer);

  if (Index == HandleCount) {
    DEBUG ((DEBUG_ERROR, "%a: no compatible nodes\n"));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}
