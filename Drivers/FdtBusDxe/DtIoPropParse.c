/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

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
  @param  Type                  Type of the field to parse out.
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
  @param  Type                  Type of the field to parse out.
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
  @param  Type                  Type of the field to parse out.
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
  @param  Type                  Type of the field to parse out.
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
  @param  Type                  Type of the field to parse out.
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
  @param  Type                  Type of the field to parse out.
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
  @param  Type                  Type of the field to parse out.
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

  AddressCells = DtDevice->DtIo.AddressCells;
  SizeCells    = DtDevice->DtIo.SizeCells;

  SetMem (Reg, sizeof (EFI_DT_REG), 0);

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

  Status = DtIoParsePropBusAddress (DtDevice, Prop, 0, &Reg->BusBase);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  Status = DtIoParsePropSize (DtDevice, Prop, 0, &Reg->Length);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  BusDevice = NULL;
  Status    = DtDeviceTranslateRangeToCpu (
                DtDevice,
                &Reg->BusBase,
                &Reg->Length,
                &Reg->TranslatedBase,
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
    Reg->BusDtIo = &BusDevice->DtIo;
  } else {
    Status = ApplyGcdTypeAndAttrs (
               Reg->TranslatedBase,
               Reg->Length,
               EfiGcdMemoryTypeMemoryMappedIo,
               EFI_MEMORY_UC,
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
  }

  return Status;
}

/**
  Parses out a ranges property value, advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
                                current position.
  @param  Type                  Type of the field to parse out.
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
  UINTN       ElemCells;
  UINTN       Cells;
  EFI_STATUS  Status;
  DT_DEVICE   *BusDevice;
  UINT8       AddressCells;
  UINT8       ChildAddressCells;
  UINT8       ChildSizeCells;
  CONST VOID  *OriginalIter;

  AddressCells      = DtDevice->DtIo.AddressCells;
  ChildAddressCells = DtDevice->DtIo.ChildAddressCells;
  ChildSizeCells    = DtDevice->DtIo.ChildSizeCells;

  SetMem (Range, sizeof (EFI_DT_RANGE), 0);

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

  Status = DtIoParsePropChildBusAddress (DtDevice, Prop, 0, &Range->ChildBase);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  Status = DtIoParsePropBusAddress (DtDevice, Prop, 0, &Range->ParentBase);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  Status = DtIoParsePropChildSize (DtDevice, Prop, 0, &Range->Length);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  BusDevice = NULL;
  Status    = DtDeviceTranslateRangeToCpu (
                DtDevice,
                &Range->ParentBase,
                &Range->Length,
                &Range->TranslatedParentBase,
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
    Range->BusDtIo = &BusDevice->DtIo;
  } else {
    Status = ApplyGcdTypeAndAttrs (
               Range->TranslatedParentBase,
               Range->Length,
               EfiGcdMemoryTypeMemoryMappedIo,
               EFI_MEMORY_UC,
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
  @param  Type                  Type of the field to parse out.
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
      break;
  }

  return EFI_UNSUPPORTED;
}
