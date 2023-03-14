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

  FreePool (DtDevice->DevicePath);
  FreePool (DtDevice->ComponentName);
  FreePool (DtDevice);
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
  @retval None

**/
EFI_STATUS
DtDeviceScan (
  IN  EFI_HANDLE               ControllerHandle,
  IN  EFI_HANDLE               DriverBindingHandle,
  IN  DT_DEVICE                *DtDevice,
  IN  EFI_DT_DEVICE_PATH_NODE  *RemainingDevicePath
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
    EFI_HANDLE   NodeHandle;
    VOID         *OpenProtoData;

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

    NodeHandle = NULL;
    Status     = gBS->InstallMultipleProtocolInterfaces (
                        &NodeHandle,
                        &gEfiDevicePathProtocolGuid,
                        NodeDtDevice->DevicePath,
                        &gEfiDtIoProtocolGuid,
                        &NodeDtDevice->DtIo,
                        NULL,
                        NULL
                        );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: InstallMultipleProtocolInterfaces(%a): %r\n", __func__, Name, Status));
      DtDeviceCleanup (NodeDtDevice);
      continue;
    }

    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiDtIoProtocolGuid,
                    &OpenProtoData,
                    DriverBindingHandle,
                    NodeHandle,
                    EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER(%a): %r\n", __func__, Name, Status));
      gBS->UninstallMultipleProtocolInterfaces (
             NodeHandle,
             &gEfiDevicePathProtocolGuid,
             NodeDtDevice->DevicePath,
             &gEfiDtIoProtocolGuid,
             &NodeDtDevice->DtIo,
             NULL
             );
      DtDeviceCleanup (NodeDtDevice);
      continue;
    }
  }

  if ((Node < 0) && (Node != -FDT_ERR_NOTFOUND)) {
    DEBUG ((DEBUG_ERROR, "%a: fdt_for_each_subnode: %a\n", __func__, fdt_strerror (Node)));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}
