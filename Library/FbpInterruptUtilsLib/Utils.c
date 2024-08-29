/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/FbpUtilsLib.h>
#include <Library/FbpInterruptUtilsLib.h>
#include <Library/UefiBootServicesTableLib.h>

STATIC
BOOLEAN
IsInterruptController (
  IN  EFI_DT_IO_PROTOCOL  *This
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;

  Status = This->GetProp (This, "interrupt-controller", &Property);
  if (!EFI_ERROR (Status)) {
    return TRUE;
  }

  return FALSE;
}

STATIC
EFI_STATUS
TranslateWithInterruptNexus (
  IN  EFI_DT_IO_PROTOCOL   *Child,
  IN  EFI_DT_IO_PROTOCOL   *Nexus,
  IN  UINT32               *InterruptCells,
  IN  OUT EFI_DT_PROPERTY  *Interrupt,
  OUT EFI_HANDLE           *InterruptParentHandle,
  OUT EFI_DT_IO_PROTOCOL   **InterruptParentIo
  )
{
  EFI_STATUS          Status;
  EFI_DT_PROPERTY     InterruptMap;
  EFI_DT_PROPERTY     InterruptMapMask;
  EFI_DT_BUS_ADDRESS  MaskedChildUnitAddress;
  EFI_HANDLE          ParentHandle;
  EFI_DT_IO_PROTOCOL  *Parent;
  UINT32              ParentInterruptCells;
  BOOLEAN             BadMatch;

  Status = Nexus->GetProp (Nexus, "interrupt-map", &InterruptMap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetProp(interrupt-map): %r\n", __func__, Status));
    return Status;
  }

  Status = Nexus->GetProp (Nexus, "interrupt-map-mask", &InterruptMapMask);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetProp(interrupt-map): %r\n", __func__, Status));
    return Status;
  }

  if (Child->AddressCells != 0) {
    EFI_DT_REG  Reg;

    Status = Child->GetReg (Child, 0, &Reg);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: GetReg: %r\n", __func__, Status));
      return Status;
    }

    Status = Child->ParseProp (Child, &InterruptMapMask, EFI_DT_VALUE_BUS_ADDRESS, 0, &MaskedChildUnitAddress);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: ParseProp(MaskedChildUnitAddress): %r\n", __func__, Status));
      return Status;
    }

    MaskedChildUnitAddress &= Reg.BusBase;
  }

  while (InterruptMap.Iter < InterruptMap.End) {
    //
    // interrupt-map is a table, but each row is not the same size, as each entry can have
    // a potentially different interrupt-parent and the parent unit address and interrupt
    // specifiers are in the domain of the interrupt parent. Thus we need to parse
    // every row in the table even if we know they don't match.
    //

    BadMatch = FALSE;
    if (Child->AddressCells != 0) {
      EFI_DT_BUS_ADDRESS  ChildUnitAddress;

      Status = Child->ParseProp (Child, &InterruptMap, EFI_DT_VALUE_BUS_ADDRESS, 0, &ChildUnitAddress);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: ParseProp(MaskedChildUnitAddress): %r\n", __func__, Status));
        //
        // Malfomed table...
        //
        Status = EFI_DEVICE_ERROR;
        return Status;
      }

      if (ChildUnitAddress != MaskedChildUnitAddress) {
        BadMatch = TRUE;
      }
    }

    if (!BadMatch) {
      BadMatch = !FbpPropertyCompare (Interrupt, &InterruptMap, *InterruptCells, &InterruptMapMask);
    }

    InterruptMap.Iter = ((EFI_DT_CELL *)InterruptMap.Iter) + *InterruptCells;
    if (InterruptMap.Iter > InterruptMap.End) {
      DEBUG ((DEBUG_ERROR, "%a: malformed row smaller than InterruptCells %lu\n", __func__, *InterruptCells));
      return EFI_DEVICE_ERROR;
    }

    Status = Nexus->ParseProp (Nexus, &InterruptMap, EFI_DT_VALUE_DEVICE, 0, &ParentHandle);
    if (EFI_ERROR (Status)) {
      //
      // Can't continue as we don't know how to size the following parent unit address
      // and interrupt specifiers.
      //
      DEBUG ((DEBUG_ERROR, "%a: ParseProp (ParentHandle): %r\n", __func__, Status));
      //
      // Malfomed table... or issues with device lookup (missing drivers for intermediate nodes).
      //
      Status = EFI_DEVICE_ERROR;
      return Status;
    }

    Status = gBS->HandleProtocol (ParentHandle, &gEfiDtIoProtocolGuid, (VOID **)&Parent);
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: HandleProtocol (Parent): %r\n", Status));
      //
      // Okay, this one shouldn't happen (hence assert).
      //
      Status = EFI_DEVICE_ERROR;
      return Status;
    }

    Status = Parent->GetU32 (Parent, "#interrupt-cells", 0, &ParentInterruptCells);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: GetU32(#interrupt-cells): %r\n", __func__, Status));
      return Status;
    }

    //
    // The parent unit address has no role.
    //
    InterruptMap.Iter = ((EFI_DT_CELL *)InterruptMap.Iter) + Parent->ChildAddressCells;
    if (InterruptMap.Iter > InterruptMap.End) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: malformed row smaller than parent unit address cells %lu\n",
        __func__,
        Parent->ChildAddressCells
        ));
      return EFI_DEVICE_ERROR;
    }

    if ((InterruptMap.End - InterruptMap.Iter) / (sizeof (EFI_DT_CELL)) < ParentInterruptCells) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: malformed row smaller than parent interrupt cells %lu\n",
        __func__,
        ParentInterruptCells
        ));
      return EFI_DEVICE_ERROR;
    }

    if (!BadMatch) {
      *Interrupt             = InterruptMap;
      *InterruptParentHandle = ParentHandle;
      *InterruptParentIo     = Parent;
      *InterruptCells        = ParentInterruptCells;
      return EFI_SUCCESS;
    }

    InterruptMap.Iter = ((EFI_DT_CELL *)InterruptMap.Iter) + ParentInterruptCells;
  }

  return EFI_NOT_FOUND;
}

/**
  Looks up an interrupt for a Devicetree node associated with the
  EFI_DT_IO_PROTOCOL instance, returning the matching controller
  handle  and interrupt information necessary for handler registration.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Index                 The index of the interrupt requested.
  @param  InterruptParent       EFI_HANDLE *.
  @param  Interrupt             EFI_DT_PROPERTY to pass to *DtInterrupt.

  @return EFI_SUCCESS           *DtInterrupt and *Interrupt populated.
**/
EFI_STATUS
FbpInterruptGet (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               Index,
  OUT EFI_HANDLE          *InterruptParent,
  OUT EFI_DT_PROPERTY     *Interrupt
  )
{
  EFI_STATUS          Status;
  EFI_HANDLE          ParentHandle;
  EFI_DT_IO_PROTOCOL  *Parent;
  EFI_DT_IO_PROTOCOL  *Child;
  EFI_DT_IO_PROTOCOL  *Nexus;
  EFI_DT_PROPERTY     Interrupts;
  UINT32              InterruptCells;

  Child  = This;
  Status = Child->GetProp (Child, "interrupts", &Interrupts);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetProp(interrupts): %r\n", __func__, Status));
    return Status;
  }

  Status = Child->GetDevice (Child, "interrupt-parent", 0, &ParentHandle);
  if (EFI_ERROR (Status)) {
    ParentHandle = Child->ParentDevice;
  }

  if (ParentHandle == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: no interrupt parent\n", __func__));
    return EFI_NOT_FOUND;
  }

  Status = gBS->HandleProtocol (ParentHandle, &gEfiDtIoProtocolGuid, (VOID **)&Parent);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: HandleProtocol(EfiDtIoProtocolGuid): %r\n", __func__, Status));
    return Status;
  }

  Status = Parent->GetU32 (Parent, "#interrupt-cells", 0, &InterruptCells);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetU32(#interrupt-cells): %r\n", __func__, Status));
    return Status;
  }

  if ((Interrupts.End - Interrupts.Iter) / (sizeof (EFI_DT_CELL) * InterruptCells) <= Index) {
    DEBUG ((DEBUG_ERROR, "%a: Index %u is out of bounds\n", __func__, Index));
    return EFI_NOT_FOUND;
  }

  Interrupts.Iter = ((EFI_DT_CELL *)Interrupts.Iter) + InterruptCells * Index;
  Nexus           = Parent;

  while (!IsInterruptController (Parent)) {
    Status = TranslateWithInterruptNexus (Child, Nexus, &InterruptCells, &Interrupts, &ParentHandle, &Parent);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: TranslateWithInterruptNexus: %r\n", __func__, Status));
      return Status;
    }

    Child = Nexus;
    Nexus = Parent;
  }

  *InterruptParent = ParentHandle;
  *Interrupt       = Interrupts;

  return EFI_SUCCESS;
}
