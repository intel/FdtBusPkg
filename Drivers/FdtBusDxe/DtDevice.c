/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

LIST_ENTRY  gCriticalDevices = INITIALIZE_LIST_HEAD_VARIABLE (gCriticalDevices);

/**
  Given a EFI_DT_DEVICE_PATH_NODE, create/populate a DT_DEVICE.

  @param[in]    FdtNode          INTN.
  @param[in]    Name             CHAR8 *.
  @param[in]    Parent           DT_DEVICE *.
  @param[out]   Out              DT_DEVICE **.

  @retval EFI_SUCCESS            *Out is populated.
  @retval Others                 Errors.

**/
EFI_STATUS
DtDeviceCreate (
  IN  INTN         FdtNode,
  IN  CONST CHAR8  *Name,
  IN  DT_DEVICE    *Parent,
  IN  UINTN        DeviceFlags,
  OUT DT_DEVICE    **Out
  )
{
  DT_DEVICE                *DtDevice;
  EFI_DT_DEVICE_PATH_NODE  *NewPathNode;
  EFI_DT_DEVICE_PATH_NODE  *FullPath;
  EFI_STATUS               Status;
  BOOLEAN                  Broken;
  VOID                     *TreeBase;

  Broken   = FALSE;
  TreeBase = GetTreeBaseFromDeviceFlags (DeviceFlags);

  NewPathNode = FbpPathNodeCreate (Name);
  if (NewPathNode == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: DtDevicePathNodeCreate\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  FullPath = (VOID *)AppendDevicePathNode (
                       Parent != NULL ?
                       (VOID *)Parent->DevicePath : NULL,
                       (VOID *)NewPathNode
                       );
  FreePool (NewPathNode);
  if (FullPath == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AppendDevicePathNode\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  if (!EFI_ERROR (DtPathToHandle ((VOID *)FullPath, FALSE, NULL))) {
    return EFI_ALREADY_STARTED;
  }

  DtDevice = AllocateZeroPool (sizeof *DtDevice);
  if (DtDevice == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocateZeroPool\n", __func__));
    FreePool (FullPath);
    return EFI_OUT_OF_RESOURCES;
  }

  DtDevice->Signature = DT_DEV_SIGNATURE;

  //
  // DtDevice->Handle is filled in DtDeviceRegister().
  //
  DtDevice->FdtNode    = FdtNode;
  DtDevice->DevicePath = FullPath;
  DtDevice->Parent     = Parent;

  //
  // Properties useful to most clients.
  //
  DtDevice->DtIo.ComponentName = FormatComponentName (Name);
  DtDevice->DtIo.Name          = Name;
  DtDevice->DtIo.DeviceType    = FdtGetDeviceType (TreeBase, FdtNode);
  DtDevice->DtIo.DeviceStatus  = FdtGetStatus (TreeBase, FdtNode);
  if (DtDevice->DtIo.DeviceStatus == EFI_DT_STATUS_BROKEN) {
    DEBUG ((DEBUG_ERROR, "%a: FdtGetStatus\n", __func__));
    Broken = TRUE;
  }

  if ((Parent != NULL) && ((Parent->Flags & DT_DEVICE_HAS_ADDRESS_CELLS) != 0)) {
    DtDevice->DtIo.AddressCells = Parent->DtIo.ChildAddressCells;
  } else {
    //
    // Default value in 2.3.5 #address-cells and #size-cells.
    //
    DtDevice->DtIo.AddressCells = 2;
  }

  if ((Parent != NULL) && ((Parent->Flags & DT_DEVICE_HAS_SIZE_CELLS) != 0)) {
    DtDevice->DtIo.SizeCells = Parent->DtIo.ChildSizeCells;
  } else {
    //
    // Default value in 2.3.5 #address-cells and #size-cells.
    //
    DtDevice->DtIo.SizeCells = 1;
  }

  Status = FdtGetAddressCells (
             TreeBase,
             FdtNode,
             &DtDevice->DtIo.ChildAddressCells
             );
  if (!EFI_ERROR (Status)) {
    DtDevice->Flags |= DT_DEVICE_HAS_ADDRESS_CELLS;
  } else if (Status != EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: FdtGetAddressCells: %r\n", __func__, Status));
    Broken = TRUE;
  }

  Status = FdtGetSizeCells (
             TreeBase,
             FdtNode,
             &DtDevice->DtIo.ChildSizeCells
             );
  if (!EFI_ERROR (Status)) {
    DtDevice->Flags |= DT_DEVICE_HAS_SIZE_CELLS;
  } else if (Status != EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: FdtGetSizeCells: %r\n", __func__, Status));
    Broken = TRUE;
  }

  DtDevice->DtIo.IsDmaCoherent = FdtGetDmaCoherency (TreeBase, FdtNode);
  DtDevice->DtIo.ParentDevice  = Parent == NULL ? NULL : Parent->Handle;

  if (Broken) {
    DEBUG ((DEBUG_ERROR, "%a: marking %a as broken\n", __func__, Name));
    DtDevice->DtIo.DeviceStatus = EFI_DT_STATUS_BROKEN;
  }

  DtDevice->Flags |= DeviceFlags & DT_DEVICE_INHERITED;
  if (FdtIsDeviceCritical (TreeBase, FdtNode) ||
      (AsciiStrCmp (DtDevice->DtIo.DeviceType, "memory") == 0))
  {
    DtDevice->Flags |= DT_DEVICE_CRITICAL;
  }

  if (((DtDevice->Flags & DT_DEVICE_TEST) != 0) &&
      FdtIsUnitTestDevice (TreeBase, FdtNode))
  {
    DtDevice->Flags |= DT_DEVICE_TEST_UNIT;
  }

  if ((DtDevice->Flags & DT_DEVICE_CRITICAL) != 0) {
    InsertTailList (&gCriticalDevices, &DtDevice->Link);
  }

  //
  // Core.
  //
  DtDevice->DtIo.Lookup       = DtIoLookup;
  DtDevice->DtIo.GetProp      = DtIoGetProp;
  DtDevice->DtIo.ScanChildren = DtIoScanChildren;
  DtDevice->DtIo.RemoveChild  = DtIoRemoveChild;
  DtDevice->DtIo.SetCallbacks = DtIoSetCallbacks;

  //
  // Convenience calls.
  //
  DtDevice->DtIo.ParseProp      = DtIoParseProp;
  DtDevice->DtIo.GetStringIndex = DtIoGetStringIndex;
  DtDevice->DtIo.GetU32         = DtIoGetU32;
  DtDevice->DtIo.GetU64         = DtIoGetU64;
  DtDevice->DtIo.GetU128        = DtIoGetU128;
  DtDevice->DtIo.GetReg         = DtIoGetReg;
  DtDevice->DtIo.GetRegByName   = DtIoGetRegByName;
  DtDevice->DtIo.GetRange       = DtIoGetRange;
  DtDevice->DtIo.GetString      = DtIoGetString;
  DtDevice->DtIo.GetDevice      = DtIoGetDevice;
  DtDevice->DtIo.IsCompatible   = DtIoIsCompatible;

  //
  // Device register access.
  //
  DtDevice->DtIo.PollReg    = DtIoPollReg;
  DtDevice->DtIo.ReadReg    = DtIoReadReg;
  DtDevice->DtIo.WriteReg   = DtIoWriteReg;
  DtDevice->DtIo.CopyReg    = DtIoCopyReg;
  DtDevice->DtIo.SetRegType = DtIoSetRegType;

  //
  // DMA operations.
  //
  DtDevice->DtIo.Map            = DtIoMap;
  DtDevice->DtIo.Unmap          = DtIoUnmap;
  DtDevice->DtIo.AllocateBuffer = DtIoAllocateBuffer;
  DtDevice->DtIo.FreeBuffer     = DtIoFreeBuffer;

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

  if ((DtDevice->Flags & DT_DEVICE_CRITICAL) != 0) {
    RemoveEntryList (&DtDevice->Link);
  }

  FreePool (DtDevice->DtIo.ComponentName);
  FreePool (DtDevice->DevicePath);
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
  IN  EFI_HANDLE  ControllerHandle OPTIONAL,
  IN  EFI_HANDLE  DriverBindingHandle OPTIONAL
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
      DtDevice->DtIo.ComponentName,
      Status
      ));
    if (ControllerHandle != NULL) {
      EFI_STATUS  Status2;
      Status2 = gBS->OpenProtocol (
                       ControllerHandle,
                       &gEfiDtIoProtocolGuid,
                       &OpenProtoData,
                       DriverBindingHandle,
                       DtDevice->Handle,
                       EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                       );
      if (EFI_ERROR (Status2)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER(%s): %r\n",
          __func__,
          DtDevice->DtIo.ComponentName,
          Status2
          ));
        return Status2;
      }
    }
  }

  return Status;
}

/**
  Wrapper over DtDeviceRegister and DtDeviceCleanup.

  @param[in]    DeviceHandle        EFI_HANDLE.
  @param[in]    ParentHandle        EFI_HANDLE.
  @param[in]    DriverBindingHandle EFI_HANDLE.

  @retval EFI_SUCCESS               *Out is populated.
  @retval Others                    Errors.

**/
EFI_STATUS
DtDeviceRemove (
  IN EFI_HANDLE  DeviceHandle,
  IN EFI_HANDLE  ParentHandle,
  IN EFI_HANDLE  DriverBindingHandle
  )
{
  EFI_STATUS          Status;
  DT_DEVICE           *DtDevice;
  EFI_DT_IO_PROTOCOL  *DtIo;

  ASSERT (DriverBindingHandle != NULL);
  ASSERT (ParentHandle != NULL);

  Status = gBS->OpenProtocol (
                  DeviceHandle,
                  &gEfiDtIoProtocolGuid,
                  (VOID **)&DtIo,
                  DriverBindingHandle,
                  ParentHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    if (Status != EFI_UNSUPPORTED) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: OpenProtocol(%p): %r\n",
        __func__,
        DeviceHandle,
        Status
        ));
    }

    return Status;
  }

  DtDevice = DT_DEV_FROM_THIS (DtIo);
  Status   = DtDeviceUnregister (
               DtDevice,
               ParentHandle,
               DriverBindingHandle
               );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DtDeviceUnregister(%s): %r\n",
      __func__,
      DtIo->ComponentName,
      Status
      ));
    return Status;
  }

  DtDeviceCleanup (DtDevice);
  return EFI_SUCCESS;
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
  IN  EFI_HANDLE  ControllerHandle OPTIONAL,
  IN  EFI_HANDLE  DriverBindingHandle OPTIONAL
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
    DEBUG ((DEBUG_ERROR, "%a: InstallMultipleProtocolInterfaces(%s): %r\n", __func__, DtDevice->DtIo.ComponentName, Status));
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
    DEBUG ((DEBUG_ERROR, "%a: EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER(%s): %r\n", __func__, DtDevice->DtIo.ComponentName, Status));
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
  IN  EFI_HANDLE               DriverBindingHandle
  )
{
  INTN        Node;
  EFI_STATUS  Status;
  VOID        *TreeBase;

  ASSERT (DriverBindingHandle != NULL);

  TreeBase = GetTreeBaseFromDeviceFlags (DtDevice->Flags);

  if (RemainingDevicePath != NULL) {
    if ((DevicePathType (RemainingDevicePath) != HARDWARE_DEVICE_PATH) ||
        (DevicePathSubType (RemainingDevicePath) != HW_VENDOR_DP) ||
        !CompareGuid (
           &RemainingDevicePath->VendorDevicePath.Guid,
           &gEfiDtDevicePathGuid
           ))
    {
      //
      // Nothing to-do, as the remaining device path does not describe a DT node.
      //
      return EFI_SUCCESS;
    }
  }

  Node = -FDT_ERR_NOTFOUND;
  fdt_for_each_subnode (Node, TreeBase, DtDevice->FdtNode) {
    INT32        Len;
    CONST CHAR8  *Name;
    DT_DEVICE    *NodeDtDevice;

    Len  = 0;
    Name = fdt_get_name (TreeBase, Node, &Len);
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
               DtDevice,
               DtDevice->Flags,
               &NodeDtDevice
               );
    if (Status == EFI_ALREADY_STARTED) {
      //
      // Seen/scanned before.
      //
      continue;
    }

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
               DtDevice->Handle,
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

/**
  Given an [In, In + Length) bus address range for DtDevice, translate it going
  up the device hierarchy, stops once further translation is no longer possible, returning
  the translated address in *Out and the matching bus device in *BusDevice. *BusDevice == NULL
  means the translation went all the way up to the DT root and reflects a CPU real address.

  @param[in]    DtDevice             DtDevice to translate bus address range for.
  @param[in]    In                   Bus address range base.
  @param[in]    Length               Bus address range length.
  @param[out]   Out                  Translated bus address range base.
  @param[out]   OutDevice            DtDevice in the bus address space of which *Out is valid,
                                     or NULL.

  @retval EFI_SUCCESS                Success.
  @retval Other                      Errors.

**/
EFI_STATUS
DtDeviceTranslateRangeToCpu (
  IN  DT_DEVICE                 *DtDevice,
  IN  CONST EFI_DT_BUS_ADDRESS  *In,
  IN  CONST EFI_DT_SIZE         *Length,
  OUT EFI_DT_BUS_ADDRESS        *Out,
  OUT DT_DEVICE                 **BusDevice
  )
{
  EFI_STATUS          Status;
  EFI_DT_PROPERTY     Property;
  DT_DEVICE           *CurDevice;
  EFI_DT_BUS_ADDRESS  CurAddress;

  if ((DtDevice == NULL) || (In == NULL) || (Out == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CurAddress = *In;
  CurDevice  = DtDevice->Parent;
  while (CurDevice != NULL) {
    BOOLEAN  IsIdentity;

    IsIdentity = FALSE;
    if (CurDevice->Parent == NULL) {
      //
      // Root node has no ranges.
      //
      IsIdentity = TRUE;
    } else {
      Status = DtIoGetProp (&CurDevice->DtIo, "ranges", &Property);
      if (Status == EFI_NOT_FOUND) {
        //
        // Translation stops here.
        //
        break;
      } else if (EFI_ERROR (Status)) {
        return Status;
      }

      if (Property.End == Property.Begin) {
        IsIdentity = TRUE;
      }
    }

    if (!IsIdentity) {
      return EFI_UNSUPPORTED;
    }

    CurDevice = CurDevice->Parent;
  }

  if (CurDevice == NULL) {
    //
    // Ensure this is a valid CPU address range.
    //
    if ((CurAddress > MAX_ADDRESS) ||
        ((CurAddress + *Length - 1) > MAX_ADDRESS))
    {
      return EFI_UNSUPPORTED;
    }
  }

  *Out       = CurAddress;
  *BusDevice = CurDevice;

  return EFI_SUCCESS;
}
