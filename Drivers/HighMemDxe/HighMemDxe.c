/** @file
*  High memory node enumeration DXE driver for ARM and RISC-V.
*
*  Copyright (c) 2015-2016, Linaro Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "HighMemDxe.h"

STATIC EFI_CPU_ARCH_PROTOCOL  *mCpu;

/**
  Process each /memory node range.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
ProcessMemoryRange (
  IN  EFI_DT_REG  *Reg
  )
{
  EFI_STATUS                       Status;
  UINT64                           Attributes;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  GcdDescriptor;

  if (Reg->Length == 0) {
    return EFI_SUCCESS;
  }

  Status = gDS->GetMemorySpaceDescriptor (Reg->Base, &GcdDescriptor);
  if (EFI_ERROR (Status)) {
    //
    // This can happen if Reg->Base exceeds the Gcd limits as set
    // by the Cpu HOB. You'll get an EFI_NOT_FOUND and it's really
    // a programmer error since the Reg->Base is clearly invalid.
    //
    DEBUG ((
      DEBUG_ERROR,
      "%a: gDS->GetMemorySpaceDescriptor(0x%lx-0x%lx): %r\n",
      __func__,
      Reg->Base,
      Reg->Base + Reg->Length - 1
      ));
    return Status;
  }

  if (GcdDescriptor.GcdMemoryType != EfiGcdMemoryTypeNonExistent) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Nothing to do for 0x%lx-0x%lx\n",
      __func__,
      Reg->Base,
      Reg->Base + Reg->Length - 1
      ));
    return EFI_SUCCESS;
  }

  Status = gDS->AddMemorySpace (
                  EfiGcdMemoryTypeSystemMemory,
                  Reg->Base,
                  Reg->Length,
                  EFI_MEMORY_WB
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: gDS->AddMemorySpace(0x%lx-0x%lx): %r\n",
      __func__,
      Reg->Base,
      Reg->Base + Reg->Length - 1,
      Status
      ));
    return Status;
  }

  Status = gDS->SetMemorySpaceAttributes (
                  Reg->Base,
                  Reg->Length,
                  EFI_MEMORY_WB
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_WARN,
      "%a: gDS->SetMemorySpaceAttributes(0x%lx-0x%lx): %r\n",
      __func__,
      Reg->Base,
      Reg->Base + Reg->Length - 1,
      Status
      ));
    return Status;
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

  Status = mCpu->SetMemoryAttributes (mCpu, Reg->Base, Reg->Length, Attributes);

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: mCpu->SetMemorySpaceAttributes(0x%lx-0x%lx): %r\n",
      __func__,
      Reg->Base,
      Reg->Base + Reg->Length - 1,
      Status
      ));
  } else {
    DEBUG ((
      DEBUG_INFO,
      "%a: Add System RAM @ 0x%lx - 0x%lx\n",
      __func__,
      Reg->Base,
      Reg->Base + Reg->Length - 1
      ));
  }

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
  EFI_DT_IO_PROTOCOL                *DtIo;
  UINTN                             HandleCount;
  EFI_HANDLE                        *HandleBuffer;
  UINTN                             Index;
  EFI_DT_REG                        Reg;
  BOOLEAN                           Foundit;

  Foundit = FALSE;
#endif

  Status = gBS->LocateProtocol (
                  &gEfiCpuArchProtocolGuid,
                  NULL,
                  (VOID **)&mCpu
                  );
  ASSERT_EFI_ERROR (Status);

#ifdef DT_NON_DRIVER_BINDING
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDtIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  ASSERT_EFI_ERROR (Status);

  for (Index = 0; Index < HandleCount ; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiDtIoProtocolGuid, (VOID **) &DtIo);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: HandleProtocol gEfiDtIoProtocolGuid failed. \n",
        __func__
        ));
      continue;
    } else {
      if (AsciiStrCmp (DtIo->DeviceType, "memory") == 0) {
          Foundit = TRUE;
        //
        // found the node or nodes
        //
        Status = DtIo->GetReg (DtIo, 0, &Reg);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: DtIo->GetReg is failed. \n",
            __func__
            ));
        }

        Status = ProcessMemoryRange (&Reg);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: Can't Set Memory Attributes at node [%a]: %r\n",
            __func__,
            DtIo->Name,
            Status
            ));
        } else {
          DEBUG ((
            DEBUG_INFO,
            "%a: DT_IO Set Memory Attributes at node [%a] successfully. \n",
            __func__,
            DtIo->Name
            ));
        }
      }
    }
  }
  //
  // Can not find any Memory node
  //
  if (!Foundit) {
    DEBUG ((DEBUG_ERROR, "We can't find Memroy Node! \n"));
  }

  return EFI_SUCCESS;
#else
  return EfiLibInstallDriverBindingComponentName2 (
           ImageHandle,
           SystemTable,
           &gDriverBinding,
           ImageHandle,
           &gComponentName,
           &gComponentName2
           );
#endif
}
