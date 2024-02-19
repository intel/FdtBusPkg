/** @file
    NOR Flash Driver for the cfi-flash DT node.

    Copyright (c) 2011 - 2021, ARM Ltd. All rights reserved.<BR>
    Copyright (c) 2020, Linaro, Ltd. All rights reserved.<BR>
    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "VirtNorFlash.h"

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
    NOR_FLASH_DEVICE_PATH  *Node;

    Node = (VOID *)RemainingDevicePath;
    if ((DevicePathType (RemainingDevicePath) != HARDWARE_DEVICE_PATH) ||
        (DevicePathSubType (RemainingDevicePath) != HW_VENDOR_DP) ||
        (DevicePathNodeLength (RemainingDevicePath) != sizeof (NOR_FLASH_DEVICE_PATH)) ||
        (!CompareGuid (&(Node->Vendor.Guid), &gEfiCallerIdGuid)))
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

  Status = DtIo->IsCompatible (DtIo, "cfi-flash");
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
  UINTN                     Index;
  EFI_STATUS                Status;
  EFI_DT_IO_PROTOCOL        *DtIo;
  EFI_DEVICE_PATH_PROTOCOL  *ControllerPath;

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
    return Status;
  }

  if ((RemainingDevicePath != NULL) &&
      IsDevicePathEndType (RemainingDevicePath))
  {
    goto out;
  }

  ControllerPath = DevicePathFromHandle (ControllerHandle);
  if (ControllerPath == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: DevicePathFromHandle\n"));
    return EFI_NOT_FOUND;
  }

  Index = 0;
  do {
    EFI_DT_REG            Reg;
    EFI_PHYSICAL_ADDRESS  RegBase;
    BOOLEAN               ContainVariableStorage;

    Status = DtIo->GetReg (DtIo, Index++, &Reg);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        Index != 0 ? DEBUG_VERBOSE : DEBUG_ERROR,
        "%a: GetReg %lu: %r\n",
        __func__,
        Index,
        Status
        ));
      if ((Index != 0) && (Status == EFI_NOT_FOUND)) {
        Status = EFI_SUCCESS;
      }

      goto out;
    }

    //
    // No, you cannot use DtIo Reg* API in a runtime DXE, as
    // FdtBusDxe is not a runtime driver.
    //
    Status = FbpRegToPhysicalAddress (&Reg, &RegBase);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: couldn't translate range to CPU addresses: %r\n",
        __func__
        ));
      ASSERT_EFI_ERROR (Status);
      continue;
    }

    //
    // Disregard any flash devices that overlap with the primary FV.
    // The firmware is not updatable from inside the guest anyway.
    //
    if ((PcdGet32 (PcdOvmfFdBaseAddress) + PcdGet32 (PcdOvmfFirmwareFdSize) > RegBase) &&
        ((RegBase + Reg.Length) > PcdGet32 (PcdOvmfFdBaseAddress)))
    {
      continue;
    }

    if (PcdGet64 (PcdFlashNvStorageVariableBase64) != 0) {
      ContainVariableStorage =
        (RegBase <= PcdGet64 (PcdFlashNvStorageVariableBase64)) &&
        (PcdGet64 (PcdFlashNvStorageVariableBase64) + PcdGet32 (PcdFlashNvStorageVariableSize) <=
         RegBase + Reg.Length);
    } else {
      ContainVariableStorage =
        (RegBase <= PcdGet32 (PcdFlashNvStorageVariableBase)) &&
        (PcdGet32 (PcdFlashNvStorageVariableBase) + PcdGet32 (PcdFlashNvStorageVariableSize) <=
         RegBase + Reg.Length);
    }

    if (!ContainVariableStorage) {
      continue;
    }

    //
    // Declare the Non-Volatile storage as EFI_MEMORY_RUNTIME.
    //
    // Note: all the NOR Flash region needs to be reserved into
    // the UEFI Runtime memory; even if we only use the small
    // block region at the top of the NOR Flash. The reason is
    // when the NOR Flash memory is set into program mode, the
    // command is written as the base of the flash region
    // (i.e.: RegBase).
    //
    Status = DtIo->SetRegType (
                     DtIo,
                     &Reg,
                     EfiDtIoRegTypeMemoryMappedIo,
                     EFI_MEMORY_UC | EFI_MEMORY_RUNTIME,
                     NULL,
                     NULL
                     );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: SetRegType %lu: %r\n",
        __func__,
        Index,
        Status
        ));
      ASSERT_EFI_ERROR (Status);
      continue;
    }

    Status = ChildCreate (
               Index,
               RegBase,
               RegBase,
               Reg.Length,
               QEMU_NOR_BLOCK_SIZE,
               ControllerHandle,
               This->DriverBindingHandle,
               ControllerPath
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: ChildCreate %lu: %r\n",
        __func__,
        Index,
        Status
        ));
      ASSERT_EFI_ERROR (Status);
      continue;
    }
  } while (1);

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
