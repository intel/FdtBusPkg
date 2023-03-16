/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

VOID  *gDeviceTreeBase;

/**
  The Entry Point for FdtBusDxe driver.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
EntryPoint (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  VOID        *Hob;
  EFI_STATUS  Status;
  VOID        *DeviceTreeBase;
  DT_DEVICE   *RootDtDevice;
  INTN        RootNode;

  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64))) {
    DEBUG ((DEBUG_ERROR, "No FDT passed in to UEFI\n"));
    return EFI_NOT_FOUND;
  }

  DeviceTreeBase = (VOID *)(UINTN)*(UINT64 *)GET_GUID_HOB_DATA (Hob);
  if (fdt_check_header (DeviceTreeBase) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DTB @ %p seems corrupted?\n",
      __func__,
      DeviceTreeBase
      ));
    return EFI_NOT_FOUND;
  }

  gDeviceTreeBase = DeviceTreeBase;
  DEBUG ((DEBUG_INFO, "%a: DTB @ %p\n", __func__, gDeviceTreeBase));

  RootNode = fdt_path_offset (gDeviceTreeBase, "/");
  if (RootNode < 0) {
    DEBUG ((DEBUG_ERROR, "%a: no root found: %a\n", __func__, fdt_strerror (RootNode)));
    return EFI_NOT_FOUND;
  }

  Status = DtDeviceCreate (RootNode, "/", NULL, &RootDtDevice);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: DtDeviceCreate: %r\n", __func__, Status));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DtDeviceRegister (RootDtDevice, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: DtDeviceRegister: %r\n", __func__, Status));
    DtDeviceCleanup (RootDtDevice);
    return Status;
  }

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gDriverBinding,
             ImageHandle,
             &gComponentName,
             &gComponentName2
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "EfiLibInstallDriverBindingComponentName2: %r\n", Status));
    DtDeviceUnregister (RootDtDevice, NULL, NULL);
    DtDeviceCleanup (RootDtDevice);
  }

  return Status;
}
