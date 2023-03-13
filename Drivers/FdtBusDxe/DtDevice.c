/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

**/

#include "FdtBusDxe.h"

/**
  Given a EFI_DT_DEVICE_PATH_NODE, create/populate a DT_DEVICE.

  @param[in]    ControllerHandle EFI_HANDLE
  @param[in]    PathNode         EFI_DT_DEVICE_PATH_NODE.
  @param[out]   Out              DT_DEVICE **.

  @retval EFI_SUCCESS            *Out is populated.
  @retval Others                 Errors.

**/
EFI_STATUS
DtDeviceCreate (
  IN  EFI_HANDLE               ControllerHandle,
  IN  EFI_DT_DEVICE_PATH_NODE  *PathNode,
  OUT DT_DEVICE                **Out
  )
{
  DT_DEVICE  *DtDevice;

  DtDevice = AllocateZeroPool (sizeof *DtDevice);
  if (DtDevice == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocateZeroPool\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  DtDevice->Signature         = DT_DEV_SIGNATURE;
  DtDevice->DtIo.DeviceHandle = ControllerHandle;
  DtDevice->DtIo.Name         = PathNode->Name;
  DtDevice->ComponentName     = FormatComponentName (PathNode->Name);
  *Out                        = DtDevice;

  return EFI_SUCCESS;
}

/**
  Free a DT_DEVICE.

  @param[in]    DtDevice         DT_DEVICE *.

  @retval EFI_SUCCESS            Success.
  @retval None

**/
VOID
DtDeviceCleanup (
  IN  DT_DEVICE  *DtDevice
  )
{
  if (DtDevice == NULL) {
    return;
  }

  FreePool (DtDevice->ComponentName);
  FreePool (DtDevice);
}
