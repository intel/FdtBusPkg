/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

/**
  Parses out a fdtbuspkg,reg-attrs/fdtbuspkg,range-attrs property value,
  advancing Prop->Iter on success.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Prop                  EFI_DT_PROPERTY describing the property buffer and
  @param  Index                 Index of the field to return, starting from the
                                current buffer position within the EFI_DT_PROPERTY.
  @param  GcdType               Pointer to EFI_GCD_MEMORY_TYPE to populate.
  @param  EfiMemoryAttributes   Pointer to UINT64 to populate.
                                current position.

  @retval EFI_SUCCESS           Parsing successful.
  @retval EFI_NOT_FOUND         Not enough remaining property buffer to contain
                                the field of specified type.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
STATIC
EFI_STATUS
DtPropParseEfiTypeAndAttrs (
  IN  DT_DEVICE            *DtDevice,
  IN  OUT EFI_DT_PROPERTY  *Prop,
  IN  UINTN                Index,
  OUT EFI_GCD_MEMORY_TYPE  *GcdType,
  OUT UINT64               *EfiMemoryAttributes
  )
{
  UINTN                Cells;
  UINTN                ElemCells;
  CONST VOID           *OriginalIter;
  EFI_DT_IO_REG_TYPE   DtType;
  EFI_GCD_MEMORY_TYPE  Type;
  EFI_STATUS           Status;
  UINT64               Attributes;

  //
  // 1 cell for EFI_DT_IO_REG_TYPE.
  // 2 cells for EfiMemoryAttributes.
  //
  ElemCells    = 1 + 2;
  Cells        = (Prop->End - Prop->Iter) / sizeof (EFI_DT_CELL);
  OriginalIter = Prop->Iter;

  if ((Cells / ElemCells) <= Index) {
    return EFI_NOT_FOUND;
  }

  Prop->Iter = (EFI_DT_CELL *)Prop->Iter + ElemCells * Index;
  Status     = DtIoParseProp (&DtDevice->DtIo, Prop, EFI_DT_VALUE_U32, 0, &DtType);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  Status = DtIoParseProp (&DtDevice->DtIo, Prop, EFI_DT_VALUE_U64, 0, &Attributes);
  if (EFI_ERROR (Status)) {
    goto Out;
  }

  switch (DtType) {
    case EfiDtIoRegTypeReserved:
      Type = EfiGcdMemoryTypeReserved;
      break;
    case EfiDtIoRegTypeSystemMemory:
      Type = EfiGcdMemoryTypeSystemMemory;
      break;
    case EfiDtIoRegTypeMemoryMappedIo:
      Type = EfiGcdMemoryTypeMemoryMappedIo;
      break;
    case EfiDtIoRegTypePersistent:
      Type = EfiGcdMemoryTypePersistent;
      break;
    case EfiDtIoRegTypeMoreReliable:
      Type = EfiGcdMemoryTypeMoreReliable;
      break;
    default:
      Status = EFI_INVALID_PARAMETER;
      goto Out;
  }

Out:
  if (EFI_ERROR (Status)) {
    Prop->Iter = OriginalIter;
  } else {
    *GcdType             = Type;
    *EfiMemoryAttributes = Attributes;
  }

  return Status;
}

/**
  Return the attributes for a reg or ranges property field.

  This looks at optional fdtbuspkg,reg-attrs/fdtbuspkg,range-attrs
  properties. If these are absent, EfiGcdMemoryTypeMemoryMappedIo
  and EFI_MEMORY_UC are reported.

  Corrupt properties are treated as errors.

  @param[in]    DtDevice             DtDevice for property lookup.
  @param[in]    IsReg                TRUE if reg, FALSE if ranges.
  @param[in]    Index                Index of the reg/range field to look up for.
  @param[out]   GcdType              Pointer to EFI_GCD_MEMORY_TYPE to populate.
  @param[out]   EfiMemoryAttributes  Pointer to UINT64 to populate.

  @retval EFI_SUCCESS                Success.
  @retval Other                      Errors.

**/
EFI_STATUS
DtPropGetRegOrRangeEfiTypeAndAttrs (
  IN  DT_DEVICE            *DtDevice,
  IN  BOOLEAN              IsReg,
  IN  UINTN                Index,
  OUT EFI_GCD_MEMORY_TYPE  *GcdType,
  OUT UINT64               *EfiMemoryAttributes
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;
  CONST CHAR8      *PropertyName;

  ASSERT (DtDevice != NULL);
  ASSERT (GcdType != NULL);
  ASSERT (EfiMemoryAttributes != NULL);

  if (IsReg) {
    PropertyName = "fdtbuspkg,reg-attrs";
  } else {
    PropertyName = "fdtbuspkg,range-attrs";
  }

  Status = DtIoGetProp (&DtDevice->DtIo, PropertyName, &Property);
  if (Status == EFI_NOT_FOUND) {
    *GcdType             = EfiGcdMemoryTypeMemoryMappedIo;
    *EfiMemoryAttributes = EFI_MEMORY_UC;
    return EFI_SUCCESS;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DtIoGetProp(%a): %r\n",
      __func__,
      PropertyName,
      Status
      ));
    return Status;
  }

  Status = DtPropParseEfiTypeAndAttrs (
             DtDevice,
             &Property,
             Index,
             GcdType,
             EfiMemoryAttributes
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DtPropParseEfiTypeAndAttrs(%a, %lu): %r\n",
      __func__,
      PropertyName,
      Index,
      Status
      ));
    return Status;
  }

  return EFI_SUCCESS;
}
