/** @file

    Copyright (c) 2023-2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

/**
  Looks up a property by name.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Property to look up.
  @param  Property              Pointer to the EFI_DT_PROPERTY to fill.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetProp (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  OUT EFI_DT_PROPERTY     *Property
  )
{
  DT_DEVICE   *DtDevice;
  CONST VOID  *Buf;
  INT32       Len;
  VOID        *TreeBase;

  if ((This == NULL) || (Property == NULL) || (Name == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);
  TreeBase = GetTreeBaseFromDeviceFlags (DtDevice->Flags);
  Buf      = fdt_getprop (TreeBase, DtDevice->FdtNode, Name, &Len);
  if (Buf == NULL) {
    if (Len == -FDT_ERR_NOTFOUND) {
      return EFI_NOT_FOUND;
    }

    return EFI_DEVICE_ERROR;
  }

  Property->Begin = Buf;
  Property->Iter  = Buf;
  Property->End   = Buf + Len;

  return EFI_SUCCESS;
}

/**
  Parses out a UINT32 property value, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  U32                   Pointer to a UINT32.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropU32 (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT UINT32               *U32
  )
{
  UINTN              Cells;
  CONST EFI_DT_CELL  *Buf;

  Cells = (Prop->End - Prop->Iter) / sizeof (EFI_DT_CELL);
  Buf   = Prop->Iter;

  if (Cells <= Index) {
    return EFI_NOT_FOUND;
  }

  Buf       += Index;
  *U32       = fdt32_to_cpu (*Buf);
  Prop->Iter = Buf + 1;
  return EFI_SUCCESS;
}

/**
  Parses out a UINT64 property value, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  U64                   Pointer to a UINT64.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropU64 (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT UINT64               *U64
  )
{
  UINTN              ElemCells;
  UINT8              Iter;
  UINTN              Cells;
  CONST EFI_DT_CELL  *Buf;

  ElemCells = 2;
  Cells     = (Prop->End - Prop->Iter) / sizeof (EFI_DT_CELL);
  Buf       = Prop->Iter;

  if ((Cells / ElemCells) <= Index) {
    return EFI_NOT_FOUND;
  }

  Buf += ElemCells * Index;

  *U64 = 0;
  for (Iter = 0; Iter < ElemCells; Iter++, Buf++) {
    *U64 |= ((UINT64)fdt32_to_cpu (*Buf)) <<
            (32 * (ElemCells - (Iter + 1)));
  }

  Prop->Iter = Buf;
  return EFI_SUCCESS;
}

/**
  Parses out a bus address property value, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  BusAddress            Pointer to EFI_DT_BUS_ADDRESS.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropBusAddress (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT EFI_DT_BUS_ADDRESS   *BusAddress
  )
{
  UINTN              ElemCells;
  UINT8              Iter;
  UINTN              Cells;
  CONST EFI_DT_CELL  *Buf;
  UINT8              AddressCells;

  AddressCells = DtDevice->DtIo.AddressCells;

  //
  // Enforced in FdtGetAddressCells.
  //

  ASSERT (AddressCells <= FDT_MAX_NCELLS);

  ElemCells = AddressCells;
  Cells     = (Prop->End - Prop->Iter) / sizeof (EFI_DT_CELL);
  Buf       = Prop->Iter;

  if (ElemCells == 0) {
    return EFI_NOT_FOUND;
  }

  if ((Cells / ElemCells) <= Index) {
    return EFI_NOT_FOUND;
  }

  Buf += ElemCells * Index;

  *BusAddress = 0;
  for (Iter = 0; Iter < ElemCells; Iter++, Buf++) {
    *BusAddress |= ((EFI_DT_BUS_ADDRESS)fdt32_to_cpu (*Buf)) <<
                   (32 * (ElemCells - (Iter + 1)));
  }

  Prop->Iter = Buf;
  return EFI_SUCCESS;
}

/**
  Parses out a child bus address property value, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  BusAddress            Pointer to EFI_DT_BUS_ADDRESS.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropChildBusAddress (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT EFI_DT_BUS_ADDRESS   *BusAddress
  )
{
  UINTN              ElemCells;
  UINT8              Iter;
  UINTN              Cells;
  CONST EFI_DT_CELL  *Buf;
  UINT8              ChildAddressCells;

  ChildAddressCells = DtDevice->DtIo.ChildAddressCells;

  //
  // Enforced in FdtGetAddressCells.
  //

  ASSERT (ChildAddressCells <= FDT_MAX_NCELLS);

  ElemCells = ChildAddressCells;
  Cells     = (Prop->End - Prop->Iter) / sizeof (EFI_DT_CELL);
  Buf       = Prop->Iter;

  if (ElemCells == 0) {
    return EFI_NOT_FOUND;
  }

  if ((Cells / ElemCells) <= Index) {
    return EFI_NOT_FOUND;
  }

  Buf += ElemCells * Index;

  *BusAddress = 0;
  for (Iter = 0; Iter < ElemCells; Iter++, Buf++) {
    *BusAddress |= ((EFI_DT_BUS_ADDRESS)fdt32_to_cpu (*Buf)) <<
                   (32 * (ElemCells - (Iter + 1)));
  }

  Prop->Iter = Buf;
  return EFI_SUCCESS;
}

/**
  Parses out a size property value, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  Size                  Pointer to EFI_DT_SIZE.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropSize (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT EFI_DT_SIZE          *Size
  )
{
  UINTN              ElemCells;
  UINT8              Iter;
  UINTN              Cells;
  CONST EFI_DT_CELL  *Buf;
  UINT8              SizeCells;

  SizeCells = DtDevice->DtIo.SizeCells;

  //
  // Enforced in FdtGetSizeCells.
  //

  ASSERT (SizeCells <= FDT_MAX_NCELLS);

  ElemCells = SizeCells;
  Cells     = (Prop->End - Prop->Iter) / sizeof (EFI_DT_CELL);
  Buf       = Prop->Iter;

  if (ElemCells == 0) {
    *Size = 0;
    return EFI_SUCCESS;
  }

  if ((Cells / ElemCells) <= Index) {
    return EFI_NOT_FOUND;
  }

  Buf += ElemCells * Index;

  *Size = 0;
  for (Iter = 0; Iter < ElemCells; Iter++, Buf++) {
    *Size |= ((EFI_DT_SIZE)fdt32_to_cpu (*Buf)) <<
             (32 * (ElemCells - (Iter + 1)));
  }

  Prop->Iter = Buf;
  return EFI_SUCCESS;
}

/**
  Parses out a child size property value, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  Size                  Pointer to EFI_DT_SIZE.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropChildSize (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT EFI_DT_SIZE          *Size
  )
{
  UINTN              ElemCells;
  UINT8              Iter;
  UINTN              Cells;
  CONST EFI_DT_CELL  *Buf;
  UINT8              ChildSizeCells;

  ChildSizeCells = DtDevice->DtIo.ChildSizeCells;

  //
  // Enforced in FdtGetSizeCells.
  //

  ASSERT (ChildSizeCells <= FDT_MAX_NCELLS);

  ElemCells = ChildSizeCells;
  Cells     = (Prop->End - Prop->Iter) / sizeof (EFI_DT_CELL);
  Buf       = Prop->Iter;

  if (ElemCells == 0) {
    *Size = 0;
    return EFI_SUCCESS;
  }

  if ((Cells / ElemCells) <= Index) {
    return EFI_NOT_FOUND;
  }

  Buf += ElemCells * Index;

  *Size = 0;
  for (Iter = 0; Iter < ElemCells; Iter++, Buf++) {
    *Size |= ((EFI_DT_SIZE)fdt32_to_cpu (*Buf)) <<
             (32 * (ElemCells - (Iter + 1)));
  }

  Prop->Iter = Buf;
  return EFI_SUCCESS;
}

/**
  Parses out reg property value, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  Reg                   Pointer to EFI_DT_REG.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropReg (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT EFI_DT_REG           *Reg
  )
{
  UINTN       ElemCells;
  UINTN       Cells;
  EFI_STATUS  Status;
  DT_DEVICE   *BusDevice;
  UINT8       AddressCells;
  UINT8       SizeCells;
  CONST VOID  *OriginalIter;
  EFI_DT_REG  DtReg;

  AddressCells = DtDevice->DtIo.AddressCells;
  SizeCells    = DtDevice->DtIo.SizeCells;

  SetMem (&DtReg, sizeof (EFI_DT_REG), 0);

  //
  // Enforced in FdtGetAddressCells/FdtGetSizeCells.
  //

  ASSERT (AddressCells <= FDT_MAX_NCELLS);
  ASSERT (SizeCells <= FDT_MAX_NCELLS);

  ElemCells    = AddressCells + SizeCells;
  Cells        = (Prop->End - Prop->Iter) / sizeof (EFI_DT_CELL);
  OriginalIter = Prop->Iter;

  if (ElemCells == 0) {
    return EFI_NOT_FOUND;
  }

  if ((Cells / ElemCells) <= Index) {
    return EFI_NOT_FOUND;
  }

  Prop->Iter = (EFI_DT_CELL *)Prop->Iter + ElemCells * Index;

  Status = DtIoParsePropBusAddress (DtDevice, Prop, 0, &DtReg.BusBase);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  Status = DtIoParsePropSize (DtDevice, Prop, 0, &DtReg.Length);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  BusDevice = NULL;
  Status    = DtDeviceTranslateRangeToCpu (
                DtDevice,
                &DtReg.BusBase,
                &DtReg.Length,
                &DtReg.TranslatedBase,
                &BusDevice
                );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DtDeviceTranslateRangeToCpu: %r\n",
      __func__,
      Status
      ));
    goto Out;
  }

  if (BusDevice != NULL) {
    DtReg.BusDtIo = &BusDevice->DtIo;
  } else if (DtReg.Length != 0) {
    EFI_GCD_MEMORY_TYPE  Type;
    UINT64               Attributes;

    Status = DtPropGetRegOrRangeEfiTypeAndAttrs (
               DtDevice,
               TRUE,
               Index,
               &Type,
               &Attributes
               );
    if (EFI_ERROR (Status)) {
      goto Out;
    }

    Status = ApplyGcdTypeAndAttrs (
               DtReg.TranslatedBase,
               DtReg.Length,
               Type,
               Attributes,
               NULL,
               NULL,
               TRUE
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: ApplyGcdTypeAndAttrs: %r\n",
        __func__,
        Status
        ));
      goto Out;
    }
  }

Out:
  if (EFI_ERROR (Status)) {
    Prop->Iter = OriginalIter;
  } else {
    *Reg = DtReg;
  }

  return Status;
}

/**
  Parses out a ranges property value, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  Range                 Pointer to EFI_DT_RANGE.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropRange (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT EFI_DT_RANGE         *Range
  )
{
  UINTN         ElemCells;
  UINTN         Cells;
  EFI_STATUS    Status;
  DT_DEVICE     *BusDevice;
  UINT8         AddressCells;
  UINT8         ChildAddressCells;
  UINT8         ChildSizeCells;
  CONST VOID    *OriginalIter;
  EFI_DT_RANGE  DtRange;

  AddressCells      = DtDevice->DtIo.AddressCells;
  ChildAddressCells = DtDevice->DtIo.ChildAddressCells;
  ChildSizeCells    = DtDevice->DtIo.ChildSizeCells;

  SetMem (&DtRange, sizeof (EFI_DT_RANGE), 0);

  //
  // Enforced in FdtGetAddressCells/FdtGetSizeCells.
  //

  ASSERT (AddressCells <= FDT_MAX_NCELLS);
  ASSERT (ChildAddressCells <= FDT_MAX_NCELLS);
  ASSERT (ChildSizeCells <= FDT_MAX_NCELLS);

  ElemCells    = ChildAddressCells + AddressCells + ChildSizeCells;
  Cells        = (Prop->End - Prop->Iter) / sizeof (EFI_DT_CELL);
  OriginalIter = Prop->Iter;

  if (ElemCells == 0) {
    return EFI_NOT_FOUND;
  }

  if ((Cells / ElemCells) <= Index) {
    return EFI_NOT_FOUND;
  }

  Prop->Iter = (EFI_DT_CELL *)Prop->Iter + ElemCells * Index;

  Status = DtIoParsePropChildBusAddress (DtDevice, Prop, 0, &DtRange.ChildBase);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  Status = DtIoParsePropBusAddress (DtDevice, Prop, 0, &DtRange.ParentBase);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  Status = DtIoParsePropChildSize (DtDevice, Prop, 0, &DtRange.Length);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  BusDevice = NULL;
  Status    = DtDeviceTranslateRangeToCpu (
                DtDevice,
                &DtRange.ParentBase,
                &DtRange.Length,
                &DtRange.TranslatedParentBase,
                &BusDevice
                );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DtDeviceTranslateRangeToCpu: %r\n",
      __func__,
      Status
      ));
    goto Out;
  }

  if (BusDevice != NULL) {
    DtRange.BusDtIo = &BusDevice->DtIo;
  } else if (DtRange.Length != 0) {
    EFI_GCD_MEMORY_TYPE  Type;
    UINT64               Attributes;

    Status = DtPropGetRegOrRangeEfiTypeAndAttrs (
               DtDevice,
               FALSE,
               Index,
               &Type,
               &Attributes
               );
    if (EFI_ERROR (Status)) {
      goto Out;
    }

    Status = ApplyGcdTypeAndAttrs (
               DtRange.TranslatedParentBase,
               DtRange.Length,
               Type,
               Attributes,
               NULL,
               NULL,
               TRUE
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: ApplyGcdTypeAndAttrs: %r\n",
        __func__,
        Status
        ));
      goto Out;
    }
  }

Out:
  if (EFI_ERROR (Status)) {
    Prop->Iter = OriginalIter;
  } else {
    *Range = DtRange;
  }

  return Status;
}

/**
  Parses out string property value, advancing Prop->Iter on success.

  The returned pointer points to the actual embedded string data, it is
  not a copy.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  String                Pointer to CHAR8.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropString (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT CONST CHAR8          **String
  )
{
  UINTN        CurrentIndex;
  CONST VOID   *Iter;
  CONST CHAR8  *EndOfString;

  CurrentIndex = 0;
  Iter         = Prop->Iter;

  while (Iter < Prop->End) {
    EndOfString = AsciiStrFindEnd (Iter, Prop->End);
    if (EndOfString == NULL) {
      return EFI_NOT_FOUND;
    }

    if (Index != CurrentIndex) {
      CurrentIndex++;
      Iter = EndOfString;
    } else {
      //
      // Iter points to string of interest.
      //
      break;
    }
  }

  if (Iter < Prop->End) {
    *String    = Iter;
    Prop->Iter = EndOfString;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

/**
  Parses out a device property value, advancing Prop->Iter on success.

  The value is a phandle, which is converted to a device path and connected
  as necessary.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  Handle                Pointer to EFI_HANDLE.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
EFIAPI
DtIoParsePropDevice (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT EFI_HANDLE           *Handle
  )
{
  UINT32             Phandle;
  EFI_STATUS         Status;
  INTN               NodeOffset;
  INTN               FdtRet;
  VOID               *TreeBase;
  CHAR8              *Path;
  UINTN              PathSize;
  CONST EFI_DT_CELL  *OriginalIter;

  OriginalIter = Prop->Iter;
  Status       = DtIoParsePropU32 (DtDevice, Prop, Index, &Phandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // We have a phandle. What follows is an extremely suboptimal
  // implementation. fdt_node_offset_by_phandle is slow, plus
  // fdt_get_path builds a path (slowly, and with extra memory),
  // plus DtIoLookup then builds a device path (with tons
  // of pool churn).
  //
  // It should be possible to use fdt_get_path-like logic
  // (comparing phandles instead of node offsets) and to build
  // the DP directly. Instead of the awkward DP manipulation
  // routines, instead keep the DP nodes in a linked list and
  // flatten these when done.
  //
  // Of course, this is an optimization and may be entirely
  // unwarranted.
  //

  TreeBase   = GetTreeBaseFromDeviceFlags (DtDevice->Flags);
  NodeOffset = fdt_node_offset_by_phandle (TreeBase, Phandle);
  if (NodeOffset < 0) {
    Status = EFI_NOT_FOUND;
    goto out;
  }

  //
  // We have a node offset. Lookup a path.
  //

  Path     = NULL;
  PathSize = 0;
  FdtRet   = 0;
  do {
    PathSize += EFI_PAGE_SIZE;
    if (Path != NULL) {
      FreePool (Path);
      Path = NULL;
    }

    Path = AllocatePool (PathSize);
    if (Path == NULL) {
      break;
    }

    FdtRet = fdt_get_path (TreeBase, NodeOffset, Path, PathSize);
  } while (FdtRet == -FDT_ERR_NOSPACE);

  if (Path == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto out;
  }

  if (FdtRet < 0) {
    Status = EFI_DEVICE_ERROR;
    goto out;
  }

  //
  // Have a path string - build a UEFI path and connect to a handle.
  //
  Status = DtIoLookup (&DtDevice->DtIo, Path, TRUE, Handle);
  FreePool (Path);

out:
  if (EFI_ERROR (Status)) {
    Prop->Iter = OriginalIter;
  }

  return Status;
}

/**
  Parses out a field encoded in the property, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Type                  Type of the field to parse out.
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  Buffer                Pointer to a buffer large enough to contain the
                                parsed out field.
  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoParseProp (
  IN  EFI_DT_IO_PROTOCOL   *This,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  EFI_DT_VALUE_TYPE    Type,
  IN  UINTN                Index,
  OUT VOID                 *Buffer
  )
{
  DT_DEVICE  *DtDevice;

  if ((This == NULL) || (Prop == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  switch (Type) {
    case EFI_DT_VALUE_U32:
      return DtIoParsePropU32 (DtDevice, Prop, Index, Buffer);
    case EFI_DT_VALUE_U64:
      return DtIoParsePropU64 (DtDevice, Prop, Index, Buffer);
    case EFI_DT_VALUE_U128:
      break;
    case EFI_DT_VALUE_BUS_ADDRESS:
      return DtIoParsePropBusAddress (DtDevice, Prop, Index, Buffer);
    case EFI_DT_VALUE_CHILD_BUS_ADDRESS:
      return DtIoParsePropChildBusAddress (DtDevice, Prop, Index, Buffer);
    case EFI_DT_VALUE_SIZE:
      return DtIoParsePropSize (DtDevice, Prop, Index, Buffer);
    case EFI_DT_VALUE_CHILD_SIZE:
      return DtIoParsePropChildSize (DtDevice, Prop, Index, Buffer);
    case EFI_DT_VALUE_REG:
      return DtIoParsePropReg (DtDevice, Prop, Index, Buffer);
    case EFI_DT_VALUE_RANGE:
      return DtIoParsePropRange (DtDevice, Prop, Index, Buffer);
    case EFI_DT_VALUE_STRING:
      return DtIoParsePropString (DtDevice, Prop, Index, Buffer);
    case EFI_DT_VALUE_DEVICE:
      return DtIoParsePropDevice (DtDevice, Prop, Index, Buffer);
  }

  return EFI_UNSUPPORTED;
}
