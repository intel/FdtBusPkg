/** @file
    High memory node enumeration DXE driver for ARM and RISC-V.

    Copyright (c) 2015-2016, Linaro Ltd. All rights reserved.
    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "HighMemDxe.h"

STATIC EFI_CPU_ARCH_PROTOCOL  *mCpu;

/**
  Return whether a DtIo is supported by this driver.

  @param[in] DtIo           EFI_DT_IO_PROTOCOL *.

  @retval EFI_SUCCESS       Device is supported by driver.
  @retval EFI_UNSUPPORTED   Device is not supported.

**/
EFI_STATUS
DeviceIsSupported (
  IN  EFI_DT_IO_PROTOCOL  *DtIo
  )
{
  ASSERT (DtIo != NULL);

  if (AsciiStrCmp (DtIo->DeviceType, "memory") != 0) {
    return EFI_UNSUPPORTED;
  }

  if (DtIo->DeviceStatus != EFI_DT_STATUS_OKAY) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Process a /memory node range.

  @param[in] DtIo           EFI_DT_IO_PROTOCOL *.
  @param[in] Reg            Range to process.

  @retval EFI_SUCCESS       The range are processed.
  @retval other             Some error occured (and logged).

**/
STATIC
EFI_STATUS
ProcessMemoryRange (
  IN  EFI_DT_IO_PROTOCOL  *DtIo,
  IN  EFI_DT_REG          *Reg
  )
{
  EFI_STATUS          Status;
  UINT64              Attributes;
  EFI_DT_IO_REG_TYPE  OldType;

  ASSERT (Reg != NULL);

  if (Reg->Length == 0) {
    return EFI_SUCCESS;
  }

  Status = DtIo->SetRegType (
                   DtIo,
                   Reg,
                   EfiDtIoRegTypeSystemMemory,
                   EFI_MEMORY_WB,
                   &OldType,
                   NULL
                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: SetRegType 0x%lx-0x%lx: %r\n",
      __func__,
      (UINTN)Reg->TranslatedBase,
      (UINTN)(Reg->TranslatedBase + Reg->Length - 1),
      Status
      ));
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  if (OldType == EfiDtIoRegTypeSystemMemory) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Nothing to do for 0x%lx-0x%lx\n",
      __func__,
      (UINTN)Reg->TranslatedBase,
      (UINTN)(Reg->TranslatedBase + Reg->Length - 1)
      ));
    return EFI_SUCCESS;
  }

  //
  // Due to the ambiguous nature of the RO/XP GCD memory space attributes,
  // it is impossible to add a memory space with the XP attribute in a way
  // that does not result in the XP attribute being set on *all* UEFI
  // memory map entries that are carved from it, including code regions
  // that require executable permissions.
  //
  // So instead, we never set the RO/XP attributes in the GCD memory space
  // capabilities or attribute fields, and apply any protections directly
  // on the page table mappings by going through the cpu arch protocol.
  //
  Attributes = EFI_MEMORY_WB;
  if ((PcdGet64 (PcdDxeNxMemoryProtectionPolicy) &
       (1U << (UINT32)EfiConventionalMemory)) != 0)
  {
    Attributes |= EFI_MEMORY_XP;
  }

  Status = mCpu->SetMemoryAttributes (mCpu, Reg->TranslatedBase, Reg->Length, Attributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: mCpu->SetMemorySpaceAttributes 0x%lx-0x%lx: %r\n",
      __func__,
      (UINTN)Reg->TranslatedBase,
      (UINTN)(Reg->TranslatedBase + Reg->Length - 1),
      Status
      ));
  } else {
    DEBUG ((
      DEBUG_INFO,
      "%a: Add System RAM @ 0x%lx-0x%lx\n",
      __func__,
      (UINTN)Reg->TranslatedBase,
      (UINTN)(Reg->TranslatedBase + Reg->Length - 1)
      ));
  }

  return Status;
}

/**
  Given a DtIo, process each /memory node range.

  @param[in] DtIo           EFI_DT_IO_PROTOCOL *.

  @retval EFI_SUCCESS       All ranges are processed.
  @retval other             Some error occured (and logged).

**/
EFI_STATUS
ProcessMemoryRanges (
  IN  EFI_DT_IO_PROTOCOL  *DtIo
  )
{
  UINTN                 Index;
  EFI_DT_REG            Reg;
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  RegBase;

  ASSERT (DtIo != NULL);

  Index = 0;
  do {
    Status = DtIo->GetReg (DtIo, Index++, &Reg);
    if (EFI_ERROR (Status)) {
      if (Status != EFI_NOT_FOUND) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: GetReg(%a): %r\n",
          __func__,
          DtIo->Name,
          Status
          ));
      } else {
        Status = EFI_SUCCESS;
      }

      break;
    }

    Status = FbpRegToPhysicalAddress (&Reg, &RegBase);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: couldn't translate range to CPU addresses: %r\n",
        __func__,
        Status
        ));
      ASSERT_EFI_ERROR (Status);
      break;
    }

    Status = ProcessMemoryRange (DtIo, &Reg);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: ProcessMemoryRange(%a): %r\n",
        __func__,
        DtIo->Name,
        Status
        ));
      break;
    }
  } while (1);

  return Status;
}

/**
  The Entry Point for HighMemDxe driver.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeHighMemDxe (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

 #ifdef DT_NON_DRIVER_BINDING
  EFI_DT_IO_PROTOCOL  *DtIo;
  UINTN               HandleCount;
  EFI_HANDLE          *HandleBuffer;
  UINTN               Index;
 #endif /* DT_NON_DRIVER_BINDING */

  Status = gBS->LocateProtocol (
                  &gEfiCpuArchProtocolGuid,
                  NULL,
                  (VOID **)&mCpu
                  );
  ASSERT_EFI_ERROR (Status);

 #ifndef DT_NON_DRIVER_BINDING
  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gDriverBinding,
             ImageHandle,
             &gComponentName,
             &gComponentName2
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: EfiLibInstallDriverBindingComponentName2: %r\n", __func__, Status));
    return Status;
  }

 #else
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDtIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );

  //
  // The non-binding version has a DEPEX on FdtBusDxe, so this can't
  // happen (unless there's no DT in the system...).
  //
  ASSERT_EFI_ERROR (Status);

  for (Index = 0; Index < HandleCount; Index++) {
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
                    ImageHandle,
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

    Status = DeviceIsSupported (DtIo);
    if (EFI_ERROR (Status)) {
      //
      // Don't forget to close unsupported handles, otherwise
      // other drivers won't be able to start!
      //
      Status = gBS->CloseProtocol (
                      HandleBuffer[Index],
                      &gEfiDtIoProtocolGuid,
                      ImageHandle,
                      HandleBuffer[Index]
                      );
      ASSERT_EFI_ERROR (Status);
      continue;
    }

    Status = ProcessMemoryRanges (DtIo);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: ProcessMemoryRanges(%a): %r\n",
        __func__,
        DtIo->Name,
        Status
        ));
    }
  }

  FreePool (HandleBuffer);

 #endif /* DT_NON_DRIVER_BINDING */

  return EFI_SUCCESS;
}
