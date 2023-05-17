/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

VOID              *gDeviceTreeBase;
STATIC EFI_EVENT  mPlatformHasDeviceTreeEvent;
STATIC EFI_EVENT  mEndOfDxeEvent;

/**
  Try to connect all DtDevices in the critical device list.

  @param[in] Event          EFI_EVENT.
  @param[in] Context        Unused.

  @retval None

**/
STATIC
VOID
EFIAPI
OnEndOfDxe (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  LIST_ENTRY  *Link;
  DT_DEVICE   *DtDevice;
  EFI_STATUS  Status;

  Link = gCriticalDevices.ForwardLink;
  while (Link != &gCriticalDevices) {
    DtDevice = DT_DEV_FROM_LINK (Link);
    Link     = Link->ForwardLink;

    if (!HandleHasBoundDriver (DtDevice->Handle)) {
      Status = gBS->ConnectController (DtDevice->Handle, NULL, NULL, TRUE);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "%s: critical device not connected\n", DtDevice->DtIo.ComponentName));
      }
    }
  }
}

/**
  Install the managed DT table as an EFI configuration table if the
  "platform has device tree exposed to the OS" protocol is installed.

  @param[in] Event          EFI_EVENT.
  @param[in] Context        Unused.

  @retval None

**/
STATIC
VOID
EFIAPI
OnPlatformHasDeviceTree (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;
  VOID        *Interface;

  ASSERT (Event == mPlatformHasDeviceTreeEvent);

  Status = gBS->LocateProtocol (
                  &gEdkiiPlatformHasDeviceTreeGuid,
                  NULL,
                  &Interface
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: exposing DTB @ 0x%p to OS\n",
    __FUNCTION__,
    gDeviceTreeBase
    ));
  Status = gBS->InstallConfigurationTable (&gFdtTableGuid, gDeviceTreeBase);
  ASSERT_EFI_ERROR (Status);

  gBS->CloseEvent (Event);
  mPlatformHasDeviceTreeEvent = NULL;
}

/**
  Unregister the protocol notification callback for the "platform has
  device tree exposed to the OS" protocol.

  @retval None

**/
STATIC
VOID
UnregisterDtNotification (
  VOID
  )
{
  gBS->CloseEvent (mPlatformHasDeviceTreeEvent);
  mPlatformHasDeviceTreeEvent = NULL;
}

/**
  Register a protocol notification callback for the "platform has device
  tree exposed to the OS" protocol. The callback will install the managed
  DT table as an EFI configuration table.

  @retval EFI_SUCCESS       Callback registered.
  @retval other             Some error occured.

**/
STATIC
EFI_STATUS
RegisterDtNotification (
  VOID
  )
{
  EFI_STATUS  Status;
  VOID        *Registration;

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnPlatformHasDeviceTree,
                  NULL,
                  &mPlatformHasDeviceTreeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: CreateEvent: %r\n", __func__, Status));
    return Status;
  }

  Status = gBS->RegisterProtocolNotify (
                  &gEdkiiPlatformHasDeviceTreeGuid,
                  mPlatformHasDeviceTreeEvent,
                  &Registration
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: RegisterProtocolNotify: %r\n",
      __func__,
      Status
      ));
    gBS->CloseEvent (mPlatformHasDeviceTreeEvent);
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Unregister an EndOfDxe signal notification.

  @retval None

**/
STATIC
VOID
UnregisterEndOfDxeNotification (
  VOID
  )
{
  gBS->CloseEvent (mEndOfDxeEvent);
  mEndOfDxeEvent = NULL;
}

/**
  Register an EndOfDxe signal notification to process devices listed
  as critical.

  @retval EFI_SUCCESS       Callback registered.
  @retval other             Some error occured.

**/
STATIC
EFI_STATUS
RegisterEndOfDxeNotification (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnEndOfDxe,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &mEndOfDxeEvent
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: CreateEventEx: %r\n", __func__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Create the root handle that the bus driver will manage and register
  the bus driver binding and component protocols.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.

  @retval EFI_SUCCESS       Bus driver is successfully registered.
  @retval other             Some error occured.

**/
STATIC
EFI_STATUS
RegisterBusDriver (
  IN  EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;
  DT_DEVICE   *RootDtDevice;
  INTN        RootNode;

  ASSERT (gDeviceTreeBase != NULL);

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
             gST,
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

  return EFI_SUCCESS;
}

/**
  The Entry Point for FdtBusDxe driver.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occured.

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

  Status = RegisterDtNotification ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RegisterDtNotification: %r\n", __func__, Status));
    return Status;
  }

  Status = RegisterEndOfDxeNotification ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RegisterEndOfDxeNotification: %r\n", __func__, Status));
    UnregisterDtNotification ();
    return Status;
  }

  Status = RegisterBusDriver (ImageHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RegisterBusDriver: %r\n", __func__, Status));
    UnregisterEndOfDxeNotification ();
    UnregisterDtNotification ();
    return Status;
  }

  return EFI_SUCCESS;
}
