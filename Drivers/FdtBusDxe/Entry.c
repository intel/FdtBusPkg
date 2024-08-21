/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

VOID                  *gDeviceTreeBase;
DT_DEVICE             *gRootDtDevice;
DT_DEVICE             *gTestRootDtDevice;
STATIC EFI_EVENT      mPlatformHasDeviceTreeEvent;
STATIC EFI_EVENT      mEndOfDxeEvent;
EFI_CPU_IO2_PROTOCOL  *gCpuIo2;

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

    if (!FbpHandleHasBoundDriver (DtDevice->Handle, 0, NULL)) {
      Status = gBS->ConnectController (DtDevice->Handle, NULL, NULL, TRUE);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "%s: critical device not connected\n", DtDevice->DtIo.ComponentName));
      }
    }
  }
}

/**
  Install the managed DT table as an EFI configuration table if the
  "platform has Devicetree exposed to the OS" protocol is installed.

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

  if (gDeviceTreeBase != NULL) {
    DEBUG ((
      DEBUG_INFO,
      "%a: exposing DTB @ 0x%p to OS\n",
      __FUNCTION__,
      gDeviceTreeBase
      ));
    Status = gBS->InstallConfigurationTable (&gFdtTableGuid, gDeviceTreeBase);
    ASSERT_EFI_ERROR (Status);
  }

  gBS->CloseEvent (Event);
  mPlatformHasDeviceTreeEvent = NULL;
}

/**
  Unregister the protocol notification callback for the "platform has
  Devicetree exposed to the OS" protocol.

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
  Register a protocol notification callback for the "platform has
  Devicetree exposed to the OS" protocol. The callback will install
  the managed DT table as an EFI configuration table.

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
  Create the root handles that the bus driver will manage.

  @param[in]  DeviceFlags   Inherited DT_DEVICE flags.
  @param[out] OutDtDevice   DT_DEVICE **.

  @retval EFI_SUCCESS       Success.
  @retval other             Some error occured.

**/
STATIC
EFI_STATUS
CreateRootHandle (
  IN  UINTN      DeviceFlags,
  OUT DT_DEVICE  **OutDtDevice
  )
{
  EFI_STATUS  Status;
  DT_DEVICE   *RootDtDevice;
  INTN        RootNode;
  VOID        *TreeBase;

  TreeBase = GetTreeBaseFromDeviceFlags (DeviceFlags);
  RootNode = fdt_path_offset (TreeBase, "/");
  if (RootNode < 0) {
    DEBUG ((DEBUG_ERROR, "%a: no root found: %a\n", __func__, fdt_strerror (RootNode)));
    return EFI_NOT_FOUND;
  }

  Status = DtDeviceCreate (
             RootNode,
             GetDtRootNameFromDeviceFlags (DeviceFlags),
             NULL,
             DeviceFlags,
             &RootDtDevice
             );
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

  *OutDtDevice = RootDtDevice;
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

  if (gDeviceTreeBase) {
    Status = CreateRootHandle (0, &gRootDtDevice);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: CreateRootHandle: %r\n", __func__, Status));
      goto done;
    }
  }

  if (gTestTreeBase != NULL) {
    Status = CreateRootHandle (DT_DEVICE_TEST, &gTestRootDtDevice);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: CreateRootHandle(Test): %r\n", __func__, Status));
      goto done;
    }
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
    goto done;
  }

done:
  if (EFI_ERROR (Status)) {
    if (gTestRootDtDevice != NULL) {
      DtDeviceUnregister (gTestRootDtDevice, NULL, NULL);
      DtDeviceCleanup (gTestRootDtDevice);
      gTestRootDtDevice = NULL;
    }

    if (gRootDtDevice != NULL) {
      DtDeviceUnregister (gRootDtDevice, NULL, NULL);
      DtDeviceCleanup (gRootDtDevice);
      gRootDtDevice = NULL;
    }
  }

  return Status;
}

/**
  Validate the Devicetree pointer.

  @param[in]  DeviceTreeBase Pointer to the Devicetree to check.

  @retval EFI_SUCCESS        Devicetree validated.
  @retval other              Some error occured.

**/
STATIC
EFI_STATUS
ValidateFdt (
  IN  VOID  *DeviceTreeBase
  )
{
  UINT32                TotalSize;
  EFI_PHYSICAL_ADDRESS  Address;

 #ifndef MDEPKG_NDEBUG
  EFI_STATUS  Status;
 #endif /* MDEPKG_NDEBUG */

  if (fdt_check_header (DeviceTreeBase) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DTB @ %p seems corrupted?\n",
      __func__,
      DeviceTreeBase
      ));
    return EFI_NOT_FOUND;
  }

  Address   = (EFI_PHYSICAL_ADDRESS)DeviceTreeBase;
  TotalSize = fdt_totalsize (DeviceTreeBase);
  DEBUG ((
    DEBUG_INFO,
    "%a: DTB at 0x%lx-0x%lx\n",
    __func__,
    Address,
    Address + TotalSize - 1
    ));

 #ifndef MDEPKG_NDEBUG
  Status = RangeIsMapped (Address, TotalSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DTB range not correctly mapped: %r\n",
      Status
      ));
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

 #endif /* MDEPKG_NDEBUG */

  return EFI_SUCCESS;
}

/**
  The Entry Point for FdtBusDxe driver.

  @param[in]  ImageHandle   The firmware allocated handle for the EFI image .
  @param[in]  SystemTable   A pointer to the EFI System Table.

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
    DEBUG ((DEBUG_WARN, "No FDT passed in to UEFI\n"));
    Hob = NULL;
  }

  //
  // gEfiCpuIo2ProtocolGuid is in the Depex list.
  //
  Status = gBS->LocateProtocol (
                  &gEfiCpuIo2ProtocolGuid,
                  NULL,
                  (VOID **)&gCpuIo2
                  );
  ASSERT_EFI_ERROR (Status);

  if (Hob != NULL) {
    DeviceTreeBase = (VOID *)(UINTN)*(UINT64 *)GET_GUID_HOB_DATA (Hob);
    Status         = ValidateFdt (DeviceTreeBase);
    if (EFI_ERROR (Status)) {
      //
      // Logged in ValidateFdt.
      //
      return Status;
    }

    gDeviceTreeBase = DeviceTreeBase;
  }

  Status = TestsInit ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: TestsInit: %r\n", __func__, Status));
    return Status;
  }

  Status = RegisterDtNotification ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RegisterDtNotification: %r\n", __func__, Status));
    TestsCleanup ();
    return Status;
  }

  Status = RegisterEndOfDxeNotification ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RegisterEndOfDxeNotification: %r\n", __func__, Status));
    UnregisterDtNotification ();
    TestsCleanup ();
    return Status;
  }

  Status = RegisterBusDriver (ImageHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: RegisterBusDriver: %r\n", __func__, Status));
    UnregisterEndOfDxeNotification ();
    UnregisterDtNotification ();
    TestsCleanup ();
    return Status;
  }

  return EFI_SUCCESS;
}
