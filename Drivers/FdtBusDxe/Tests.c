/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

#ifndef MDEPKG_NDEBUG

#define TEST_DT_SIZE  4096

#define BEGIN_NODE(Buffer, Name)  do {          \
    INTN  FdtErr;                               \
                                                \
    FdtErr = fdt_begin_node (Buffer, #Name);    \
    if (FdtErr < 0) {                           \
      DEBUG ((                                  \
               DEBUG_ERROR,                     \
               "%a: fdt_begin_node(%a): %a\n",  \
               __func__,                        \
               #Name,                           \
               fdt_strerror (FdtErr)            \
               ));                              \
      return FdtErr;                            \
    }                                           \
  } while (0)

#define END_NODE(Buffer, Name)  do {            \
    INTN  FdtErr;                               \
                                                \
    FdtErr =  fdt_end_node (Buffer);            \
    if (FdtErr < 0) {                           \
      DEBUG ((                                  \
               DEBUG_ERROR,                     \
               "%a: fdt_end_node(%a): %a\n",    \
               __func__,                        \
               #Name,                           \
               fdt_strerror (FdtErr)            \
               ));                              \
      return FdtErr;                            \
    }                                           \
  } while (0)

//
// Encode the pointer as a 32-bit offset to not have to worry
// when we go 128-bit.
//

#define NEW_NODE(Buffer, Name, Test, ...)  do {                         \
    BEGIN_NODE (Buffer, Name);                                          \
    if (Test != NULL) {                                                 \
      fdt_property_u32 (Buffer, "uefi,FdtBusDxe-Test", (UINTN) Test - (UINTN) TestsInit); \
    }                                                                   \
    __VA_ARGS__;                                                        \
    END_NODE (Buffer, Name);                                            \
  } while (0)

VOID  *gTestTreeBase;

/**
  Test node test.

  @param[in] DtDevice       Device to test.

  @retval TRUE              Success.
  @retval FALSE             Faulure.

**/
STATIC
BOOLEAN
EFIAPI
Test1Fn (
  IN DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->IsCompatible (DtIo, "test1_compatible") == EFI_SUCCESS);
  ASSERT (!DtIo->IsDmaCoherent);
  return TRUE;
}

/**
  Test node test.

  @param[in] DtDevice       Device to test.

  @retval TRUE              Success.
  @retval FALSE             Faulure.

**/
STATIC
BOOLEAN
EFIAPI
Test2Fn (
  IN DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->IsDmaCoherent);
  return TRUE;
}

/**
  Populate test DT nodes.

  @param[in] Buffer         Test DT to populate.

  @retval 0                 Success.
  @retval other             Some libfdt error occured.

**/
STATIC
INTN
TestsPopulate (
  VOID  *Buffer
  )
{
  NEW_NODE (
    Buffer,
    Test1,
    Test1Fn,
    fdt_property_string (Buffer, "compatible", "test1_compatible")
    );

  NEW_NODE (
    Buffer,
    Test2,
    Test2Fn,
    fdt_property (Buffer, "dma-coherent", NULL, 0)
    );

  return 0;
}

/**
  Invoke a DT node test, if present.

  @param[in] DtDevice       Node to test.

  @retval None

**/
VOID
TestsInvoke (
  IN DT_DEVICE  *DtDevice
  )
{
  CONST VOID      *Buf;
  INT32           Len;
  BOOLEAN EFIAPI  (*Fn)(
    DT_DEVICE  *DtDevice
    );

  Buf = fdt_getprop (gTestTreeBase, DtDevice->FdtNode, "uefi,FdtBusDxe-Test", &Len);
  if (Buf == NULL) {
    ASSERT (Len == -FDT_ERR_NOTFOUND);
    return;
  }

  ASSERT (Len == sizeof (UINT32));
  Fn = (VOID *)((INT32)fdt32_to_cpu (*((UINT32 *)Buf)) + (UINTN)TestsInit);
  DEBUG ((DEBUG_ERROR, "%a: running unit test\n", DtDevice->DtIo.Name));
  ASSERT (Fn (DtDevice));
}

/**
  Initialize the unit tests (which use their own device tree).

  @retval EFI_SUCCESS       Success.
  @retval other             Some error occured.

**/
EFI_STATUS
TestsInit (
  VOID
  )
{
  VOID  *Buffer;
  INTN  FdtErr;

  Buffer = AllocateZeroPool (TEST_DT_SIZE);
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocateZeroPool\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  FdtErr = fdt_create (Buffer, TEST_DT_SIZE);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_create: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = fdt_finish_reservemap (Buffer);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_finish_reservemap: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = fdt_begin_node (Buffer, "");
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_begin_node: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = TestsPopulate (Buffer);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: TestsPopulate: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr =  fdt_end_node (Buffer);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_end_node: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = fdt_finish (Buffer);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_finish: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = fdt_open_into (Buffer, Buffer, TEST_DT_SIZE);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_open_into: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  gTestTreeBase = Buffer;
done:
  if (FdtErr < 0) {
    FreePool (Buffer);
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Cleanup the unit tests (which use their own device tree).

  @retval None.

**/
VOID
TestsCleanup (
  VOID
  )
{
  FreePool (gTestTreeBase);
  gTestTreeBase = NULL;
}

#endif /* MDEPKG_NDEBUG */
