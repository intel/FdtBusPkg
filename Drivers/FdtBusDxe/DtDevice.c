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
  IN  INTN                     FdtNode,
  IN  CONST CHAR8              *Name,
  IN  EFI_DT_DEVICE_PATH_NODE  *ParentPath,
  OUT DT_DEVICE                **Out
  )
{
  DT_DEVICE                *DtDevice;
  EFI_DT_DEVICE_PATH_NODE  *NewPathNode;

  DtDevice = AllocateZeroPool (sizeof *DtDevice);
  if (DtDevice == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocateZeroPool\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  DtDevice->Signature     = DT_DEV_SIGNATURE;
  DtDevice->FdtNode       = FdtNode;
  DtDevice->ComponentName = FormatComponentName (Name);
  DtDevice->DtIo.Name     = Name;

  NewPathNode = DtDevicePathNodeCreate (Name);
  if (NewPathNode == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: DtDevicePathNodeCreate\n", __func__));
    FreePool (DtDevice);
    return EFI_OUT_OF_RESOURCES;
  }

  DtDevice->DevicePath = (VOID *)AppendDevicePathNode (
                                   (VOID *)ParentPath,
                                   (VOID *)NewPathNode
                                   );
  FreePool (NewPathNode);
  if (DtDevice->DevicePath == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AppendDevicePathNode\n", __func__));
    FreePool (DtDevice);
    return EFI_OUT_OF_RESOURCES;
  }

  *Out = DtDevice;
  return EFI_SUCCESS;
}

/**
  Free a DT_DEVICE.

  @param[in]    DtDevice         DT_DEVICE *.

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

  FreePool (DtDevice->DevicePath);
  FreePool (DtDevice->ComponentName);
  FreePool (DtDevice);
}

/**
  Undoes DtDeviceRegister.

  @param[in]    DtDevice             DT_DEVICE to unregister.

  @retval EFI_SUCCESS                Success.
  @retval Other                      Errors.

**/
EFI_STATUS
DtDeviceUnregister (
  IN  DT_DEVICE   *DtDevice,
  IN  EFI_HANDLE  ControllerHandle,
  IN  EFI_HANDLE  DriverBindingHandle
  )
{
  EFI_STATUS  Status;
  VOID        *OpenProtoData;

  if (ControllerHandle != NULL) {
    ASSERT (DriverBindingHandle != NULL);
    Status = gBS->CloseProtocol (
                    ControllerHandle,
                    &gEfiDtIoProtocolGuid,
                    DriverBindingHandle,
                    DtDevice->Handle
                    );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: CloseProtocol: %r\n", __func__, Status));
      return Status;
    }
  }

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  DtDevice->Handle,
                  &gEfiDevicePathProtocolGuid,
                  DtDevice->DevicePath,
                  &gEfiDtIoProtocolGuid,
                  &DtDevice->DtIo,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: UninstallMultipleProtocolInterfaces(%s): %r\n",
      __func__,
      DtDevice->ComponentName,
      Status
      ));
    if (ControllerHandle != NULL) {
      gBS->OpenProtocol (
             ControllerHandle,
             &gEfiDtIoProtocolGuid,
             &OpenProtoData,
             DriverBindingHandle,
             DtDevice->Handle,
             EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
             );
    }
  }

  return Status;
}

/**
  Given a DT_DEVICE, create a handle, installing relevant protocols on it
  (device path and DT I/O). If a controller handle is provided, register
  the handle a child of that.

  @param[in]    DtDevice             DT_DEVICE to register.
  @param[in]    ControllerHandle     EFI_HANDLE.
  @param[in]    DriverBindingHandle  EFI_HANDLE.

  @retval EFI_SUCCESS                Success.
  @retval Other                      Errors.

**/
EFI_STATUS
DtDeviceRegister (
  IN  DT_DEVICE   *DtDevice,
  IN  EFI_HANDLE  ControllerHandle,
  IN  EFI_HANDLE  DriverBindingHandle
  )
{
  EFI_STATUS  Status;
  VOID        *OpenProtoData;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &DtDevice->Handle,
                  &gEfiDevicePathProtocolGuid,
                  DtDevice->DevicePath,
                  &gEfiDtIoProtocolGuid,
                  &DtDevice->DtIo,
                  NULL,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InstallMultipleProtocolInterfaces(%s): %r\n", __func__, DtDevice->ComponentName, Status));
    return Status;
  }

  if (ControllerHandle == NULL) {
    return EFI_SUCCESS;
  }

  ASSERT (DriverBindingHandle != NULL);
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDtIoProtocolGuid,
                  &OpenProtoData,
                  DriverBindingHandle,
                  DtDevice->Handle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER(%s): %r\n", __func__, DtDevice->ComponentName, Status));
    gBS->UninstallMultipleProtocolInterfaces (
           DtDevice->Handle,
           &gEfiDevicePathProtocolGuid,
           DtDevice->DevicePath,
           &gEfiDtIoProtocolGuid,
           &DtDevice->DtIo,
           NULL
           );
  }

  return Status;
}

/**
  Create child handles for a DT_DEVICE. Called from DriverStart. If
  RemainingDevicePath is present and describes a DT DevicePath, only create
  this particular matching child handle.

  @param[in]    ControllerHandle     EFI_HANDLE.
  @param[in]    DriverBindingHandle  EFI_HANDLE.
  @param[in]    DtDevice             DT_DEVICE * for ControllerHandle.
  @param[in]    RemainingDevicePath  Potential child EFI_DT_DEVICE_PATH_NODE *.

  @retval EFI_SUCCESS                Success.
  @retval Other                      Errors.

**/
EFI_STATUS
DtDeviceScan (
  IN  DT_DEVICE                *DtDevice,
  IN  EFI_DT_DEVICE_PATH_NODE  *RemainingDevicePath,
  IN  EFI_HANDLE               ControllerHandle,
  IN  EFI_HANDLE               DriverBindingHandle
  )
{
  INTN        Node;
  EFI_STATUS  Status;

  if (RemainingDevicePath != NULL) {
    if ((RemainingDevicePath->VendorDevicePath.Header.Type != HARDWARE_DEVICE_PATH) ||
        (RemainingDevicePath->VendorDevicePath.Header.SubType != HW_VENDOR_DP) ||
        !CompareGuid (&RemainingDevicePath->VendorDevicePath.Guid, &gEfiDtIoProtocolGuid))
    {
      //
      // Nothing to-do, as the remaining device path does not describe a DT node.
      //
      return EFI_SUCCESS;
    }
  }

  Node = -FDT_ERR_NOTFOUND;
  fdt_for_each_subnode (Node, gDeviceTreeBase, DtDevice->FdtNode) {
    INT32        Len;
    CONST CHAR8  *Name;
    DT_DEVICE    *NodeDtDevice;

    Len  = 0;
    Name = fdt_get_name (gDeviceTreeBase, Node, &Len);
    if (Len < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: fdt_get_name(%ld): %a\n",
        __func__,
        Node,
        fdt_strerror (Len)
        ));
      continue;
    }

    if ((RemainingDevicePath != NULL) &&
        (AsciiStrCmp (RemainingDevicePath->Name, Name) != 0))
    {
      DEBUG ((
        DEBUG_VERBOSE,
        "%a: looking for %a, skipping %a\n",
        __func__,
        Name,
        RemainingDevicePath->Name
        ));
      continue;
    }

    Status = DtDeviceCreate (
               Node,
               Name,
               DtDevice->DevicePath,
               &NodeDtDevice
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: DtDeviceCreate(%a): %r\n",
        __func__,
        Name,
        Status
        ));
      continue;
    }

    Status = DtDeviceRegister (
               NodeDtDevice,
               ControllerHandle,
               DriverBindingHandle
               );
    if (EFI_ERROR (Status)) {
      DtDeviceCleanup (NodeDtDevice);
    }
  }

  if ((Node < 0) && (Node != -FDT_ERR_NOTFOUND)) {
    DEBUG ((DEBUG_ERROR, "%a: fdt_for_each_subnode: %a\n", __func__, fdt_strerror (Node)));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}
