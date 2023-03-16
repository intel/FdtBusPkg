/** @file
*  Virtio FDT client protocol driver for virtio,mmio DT node
*
*  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "VirtioFdtDxe.h"

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Because ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
STATIC
EFI_STATUS
EFIAPI
DriverSupported (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS          Status;
  EFI_DT_IO_PROTOCOL  *DtIo;

  DtIo   = NULL;
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDtIoProtocolGuid,
                  (VOID **)&DtIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_ALREADY_STARTED) {
      return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED;
  }

  Status = DtIo->IsCompatible (DtIo, "virtio,mmio");

  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiDtIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );

  return Status;
}

/**
  Register an Virtio transport child handle.

  @param[in]    RegBase              Virtio register base.
  @param[in]    ControllerHandle     Parent handle.
  @param[in]    DriverBindingHandle  Driver binding handle.
  @param[in]    ControllerPath       Parent path.

  @retval EFI_SUCCESS                Success.
  @retval Others                     Errors.

**/
STATIC
EFI_STATUS
ChildCreate (
  IN  UINTN                     RegBase,
  IN  EFI_HANDLE                ControllerHandle,
  IN  EFI_HANDLE                DriverBindingHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL  *ControllerPath
  )
{
  EFI_HANDLE                         Handle;
  EFI_STATUS                         Status;
  VOID                               *OpenProtoData;
  EFI_DEVICE_PATH_PROTOCOL           *HandlePath;
  VIRTIO_TRANSPORT_DEVICE_PATH_NODE  *PathNode;

  Handle     = NULL;
  HandlePath = NULL;

  PathNode = (VIRTIO_TRANSPORT_DEVICE_PATH_NODE *)CreateDeviceNode (
                                                    HARDWARE_DEVICE_PATH,
                                                    HW_VENDOR_DP,
                                                    sizeof (VIRTIO_TRANSPORT_DEVICE_PATH_NODE)
                                                    );
  if (PathNode == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: CreateDeviceNode: %r\n", __func__, Status));
    goto out;
  }

  CopyGuid (&PathNode->Vendor.Guid, &gVirtioMmioTransportGuid);
  PathNode->PhysBase = RegBase;

  HandlePath = AppendDevicePathNode (
                 ControllerPath,
                 (VOID *)PathNode
                 );
  FreePool (PathNode);
  if (HandlePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((
      DEBUG_ERROR,
      "%a: AppendDevicePathNode: %r\n",
      __func__,
      Status
      ));
    goto out;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiDevicePathProtocolGuid,
                  HandlePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    if (Status != EFI_ALREADY_STARTED) {
      DEBUG ((DEBUG_ERROR, "%a: CreateDeviceNode: %r\n", __func__, Status));
    }

    goto out;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDtIoProtocolGuid,
                  &OpenProtoData,
                  DriverBindingHandle,
                  Handle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER: %r\n", __func__, Status));
    goto out;
  }

  Status = VirtioMmioInstallDevice (RegBase, Handle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: VirtioMmioInstallDevice: %r\n", __func__, Status));
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiDtIoProtocolGuid,
           DriverBindingHandle,
           Handle
           );
    goto out;
  }

out:
  if (EFI_ERROR (Status)) {
    if (Handle != NULL) {
      gBS->UninstallMultipleProtocolInterfaces (
             Handle,
             &gEfiDevicePathProtocolGuid,
             HandlePath,
             NULL
             );
    }

    if (HandlePath != NULL) {
      FreePool (HandlePath);
    }
  }

  return Status;
}

/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed, or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failed to start the device.

**/
STATIC
EFI_STATUS
EFIAPI
DriverStart (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_DT_REG                Reg;
  EFI_STATUS                Status;
  EFI_DT_IO_PROTOCOL        *DtIo;
  BOOLEAN                   MyOpen;
  EFI_DEVICE_PATH_PROTOCOL  *ControllerPath;

  DtIo   = NULL;
  MyOpen = TRUE;

  ControllerPath = DevicePathFromHandle (ControllerHandle);
  if (ControllerPath == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: DevicePathFromHandle\n"));
    return EFI_NOT_FOUND;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDtIoProtocolGuid,
                  (VOID **)&DtIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status == EFI_ALREADY_STARTED) {
    //
    // Don't accidentally close EFI_DT_IO_PROTOCOL if
    // we didn't open it.
    //
    MyOpen = FALSE;
    Status = EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((RemainingDevicePath != NULL) &&
      IsDevicePathEndType (RemainingDevicePath))
  {
    goto out;
  }

  Status = DtIo->GetReg (DtIo, 0, &Reg);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetReg: %r\n", __func__, Status));
    goto out;
  }

  Status = ChildCreate (
             Reg.Base,
             ControllerHandle,
             This->DriverBindingHandle,
             ControllerPath
             );
  if (EFI_ERROR (Status)) {
    if (Status != EFI_ALREADY_STARTED) {
      DEBUG ((DEBUG_ERROR, "%a: ChildCreate: %r\n", __func__, Status));
    } else {
      Status = EFI_SUCCESS;
    }

    goto out;
  }

out:
  if (EFI_ERROR (Status)) {
    if (MyOpen) {
      gBS->CloseProtocol (
             ControllerHandle,
             &gEfiDtIoProtocolGuid,
             This->DriverBindingHandle,
             ControllerHandle
             );
    }
  }

  return Status;
}

/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed, or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.

**/
STATIC
EFI_STATUS
EFIAPI
DriverStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

GLOBAL_REMOVE_IF_UNREFERENCED EFI_DRIVER_BINDING_PROTOCOL  gDriverBinding = {
  DriverSupported,
  DriverStart,
  DriverStop,
  0xa,
  NULL,
  NULL
};
