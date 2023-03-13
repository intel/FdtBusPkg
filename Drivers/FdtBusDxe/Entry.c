/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

**/

#include "FdtBusDxe.h"

typedef struct {
  VENDOR_DEVICE_PATH    VendorDevicePath;
  CHAR8                 Name[2];
} EFI_DT_ROOT_DEVICE_PATH_NODE;

typedef struct {
  EFI_DT_ROOT_DEVICE_PATH_NODE    Node;
  EFI_DEVICE_PATH_PROTOCOL        EndDevicePath;
} EFI_DT_ROOT_DEVICE_PATH;

STATIC EFI_DT_ROOT_DEVICE_PATH  mRootDevicePath = {
  {
    {
      {
        HARDWARE_DEVICE_PATH,
        HW_VENDOR_DP,
        {
          (UINT8)(sizeof (EFI_DT_ROOT_DEVICE_PATH_NODE)),
          (UINT8)((sizeof (EFI_DT_ROOT_DEVICE_PATH_NODE)) >> 8),
        }
      },
      EFI_DT_IO_PROTOCOL_GUID
    },
    {
      '/', '\0'
    }
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      sizeof (EFI_DEVICE_PATH_PROTOCOL),
      0
    }
  }
};

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
  EFI_HANDLE  RootHandle;

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

  RootHandle = NULL;
  Status     = gBS->InstallMultipleProtocolInterfaces (
                      &RootHandle,
                      &gEfiDevicePathProtocolGuid,
                      &mRootDevicePath,
                      NULL,
                      NULL
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "InstallMultipleProtocolInterfaces: %r\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "Root handle is %p\n", RootHandle));

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
           &mRootDevicePath,
           NULL
           );
  }

  return Status;
}
