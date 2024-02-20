/** @file
    Sample DT controller device driver code.

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Driver.h"

/**
  Service child device EFI_DT_REG reads.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Width                 Signifies the width of the I/O operations.
  @param  Reg                   Pointer to a register space descriptor.
  @param  Offset                The offset within the selected register space to start the
                                I/O operation.
  @param  Count                 The number of I/O operations to perform.
  @param  Buffer                The destination buffer to store the results.

  @retval EFI_SUCCESS           The data was read from the device.
  @retval EFI_UNSUPPORTED       The address range specified by Offset, Width, and Count is not
                                valid for the register space specified by Reg.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
ReadChildReg (
  IN     EFI_DT_IO_PROTOCOL        *This,
  IN     EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN     EFI_DT_REG                *Reg,
  IN     EFI_DT_SIZE               Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  )
{
  EFI_PHYSICAL_ADDRESS  Address;

  if ((Width == EfiDtIoWidthFifoUint8) ||
      (Width == EfiDtIoWidthFifoUint16) ||
      (Width == EfiDtIoWidthFifoUint32) ||
      (Width == EfiDtIoWidthFifoUint64) ||
      (Width == EfiDtIoWidthFillUint8) ||
      (Width == EfiDtIoWidthFillUint16) ||
      (Width == EfiDtIoWidthFillUint32) ||
      (Width == EfiDtIoWidthFillUint64))
  {
    return EFI_UNSUPPORTED;
  }

  //
  // Reads return a test pattern to easily identify
  // access widths and offsets.
  //

  Address = (EFI_PHYSICAL_ADDRESS)Buffer;
  while (Count--) {
    switch (DT_IO_PROTOCOL_WIDTH (Width)) {
      case 1:
        *(UINT8 *)Address = Offset & 0xff;
        break;
      case 2:
        *(UINT16 *)Address = 0x2200 + (Offset & 0xff);
        break;
      case 4:
        *(UINT32 *)Address = 0x44444400 + (Offset & 0xff);
        break;
      case 8:
        *(UINT64 *)Address = 0x8888888888888800 + (Offset & 0xff);
        break;
      default:
        return EFI_UNSUPPORTED;
    }

    Address += DT_IO_PROTOCOL_WIDTH (Width);
    Offset  += DT_IO_PROTOCOL_WIDTH (Width);
  }

  return EFI_SUCCESS;
}

STATIC EFI_DT_IO_PROTOCOL_CB  Callbacks = {
  .ReadChildReg = ReadChildReg,
};

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

  //
  // Follows the logic described in "Bus Driver that creates
  // all of its child handles on the first call to Start()".
  //

  if ((RemainingDevicePath != NULL) &&
      !IsDevicePathEnd (RemainingDevicePath))
  {
    EFI_DT_DEVICE_PATH_NODE  *Node;

    Node = (VOID *)RemainingDevicePath;
    if ((DevicePathType (RemainingDevicePath) != HARDWARE_DEVICE_PATH) ||
        (DevicePathSubType (RemainingDevicePath) != HW_VENDOR_DP) ||
        !CompareGuid (&Node->VendorDevicePath.Guid, &gEfiDtDevicePathGuid))
    {
      return EFI_UNSUPPORTED;
    }
  }

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
    return Status;
  }

  Status = DtIo->IsCompatible (DtIo, "fdtbuspkg,sample-bus");
  if (EFI_ERROR (Status)) {
    goto out;
  }

  if (DtIo->DeviceStatus != EFI_DT_STATUS_OKAY) {
    Status = EFI_UNSUPPORTED;
    goto out;
  }

out:
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiDtIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );

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
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.
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
  EFI_STATUS          Status;
  EFI_DT_IO_PROTOCOL  *DtIo;

  //
  // Follows the logic described in "Bus Driver that
  // creates all of its child handles on the first call to Start()".
  //

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
    DEBUG ((
      DEBUG_ERROR,
      "%a: OpenProtocol: %r\n",
      __func__,
      Status
      ));
    ASSERT (Status != EFI_ALREADY_STARTED);
    return Status;
  }

  if ((RemainingDevicePath != NULL) &&
      IsDevicePathEndType (RemainingDevicePath))
  {
    goto out;
  }

  //
  // In this example, set I/O callbacks for child device reads.
  //
  Status = DtIo->SetCallbacks (DtIo, This->DriverBindingHandle, &Callbacks);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: SetCallbacks: %r\n",
      __func__,
      ControllerHandle,
      Status
      ));
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  //
  // Create all children, which is why RemainingDevicePath
  // is not being passed in.
  //
  Status = DtIo->ScanChildren (DtIo, This->DriverBindingHandle, NULL);
  if (Status == EFI_NOT_FOUND) {
    //
    // No children is not a bug.
    //
    Status = EFI_SUCCESS;
  }

  //
  // Do stuff here.
  //

out:
  if (EFI_ERROR (Status)) {
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiDtIoProtocolGuid,
           This->DriverBindingHandle,
           ControllerHandle
           );
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
  UINTN               Index;
  EFI_STATUS          Status;
  BOOLEAN             AllChildrenStopped;
  EFI_DT_IO_PROTOCOL  *DtIo;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDtIoProtocolGuid,
                  (VOID **)&DtIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: OpenProtocol(%p): %r\n",
      __func__,
      ControllerHandle,
      Status
      ));
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  if (NumberOfChildren == 0) {
    Status = DtIo->SetCallbacks (DtIo, This->DriverBindingHandle, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: SetCallbacks: %r\n",
        __func__,
        ControllerHandle,
        Status
        ));
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    return gBS->CloseProtocol (
                  ControllerHandle,
                  &gEfiDtIoProtocolGuid,
                  This->DriverBindingHandle,
                  ControllerHandle
                  );
  }

  AllChildrenStopped = TRUE;

  for (Index = 0; Index < NumberOfChildren; Index++) {
    Status = DtIo->RemoveChild (DtIo, ChildHandleBuffer[Index], This->DriverBindingHandle);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: RemoveChild(%p): %r\n",
        __func__,
        ChildHandleBuffer[Index],
        Status
        ));
      AllChildrenStopped = FALSE;
      continue;
    }
  }

  if (!AllChildrenStopped) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

GLOBAL_REMOVE_IF_UNREFERENCED EFI_DRIVER_BINDING_PROTOCOL  gDriverBinding = {
  DriverSupported,
  DriverStart,
  DriverStop,
  0xa,
  NULL,
  NULL
};
