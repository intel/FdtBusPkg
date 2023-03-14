/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

**/

#include "FdtBusDxe.h"

/**
  Given an FdtNode, return the model name property or the empty string.

  @param[in]    FdtNode          INTN

  @retval CHAR8 *                Model name or empty string.

**/
CONST CHAR8 *
FdtGetModel (
  IN INTN  FdtNode
  )
{
  CONST CHAR8  *Buf;

  Buf = fdt_getprop (gDeviceTreeBase, FdtNode, "model", NULL);
  if (Buf == NULL) {
    return "";
  }

  return Buf;
}
