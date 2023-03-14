/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

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
  VOID                       *Hob;
  EFI_STATUS                 Status;
  VOID                       *DeviceTreeBase;
  EFI_HANDLE                 RootHandle;
  EFI_DT_NODE_DATA_PROTOCOL  *RootNodeData;
  INTN                       RootNode;

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

  RootNodeData = DtNodeDataCreate ("/", NULL, RootNode);
  if (RootNodeData == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: DtNodeDataCreate\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  RootHandle = NULL;
  Status     = gBS->InstallMultipleProtocolInterfaces (
                      &RootHandle,
                      &gEfiDevicePathProtocolGuid,
                      RootNodeData->DevicePath,
                      &gEfiDtNodeDataProtocol,
                      RootNodeData,
                      NULL,
                      NULL
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InstallMultipleProtocolInterfaces: %r\n", __func__, Status));
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
    gBS->UninstallMultipleProtocolInterfaces (
           RootHandle,
           &gEfiDevicePathProtocolGuid,
           RootNodeData->DevicePath,
           &gEfiDtNodeDataProtocol,
           RootNodeData,
           NULL
           );
  }

  return Status;
}
