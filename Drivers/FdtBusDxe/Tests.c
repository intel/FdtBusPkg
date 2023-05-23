/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

#ifndef MDEPKG_NDEBUG

#define TEST_DT_SIZE  4096

//
// Encode the pointer as a 32-bit offset to not have to worry
// when we go 128-bit.
//

#define NODE_TEST(Buffer, Test, ...)  do {                              \
    if (Test != NULL) {                                                 \
      fdt_property_u32 (Buffer, "uefi,FdtBusDxe-Test", (UINTN) Test - (UINTN) TestsInit); \
    }                                                                   \
  } while (0)

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

#define NEW_NODE(Buffer, Name, Test, ...)  do {                         \
    BEGIN_NODE (Buffer, Name);                                          \
    NODE_TEST (Buffer, Test);                                           \
    __VA_ARGS__;                                                        \
    END_NODE (Buffer, Name);                                            \
  } while (0)

VOID  *gTestTreeBase;

/**
  Root node test.

  @param[in] DtDevice       Device to test.

  @retval TRUE              Success.
  @retval FALSE             Faulure.

**/
STATIC
BOOLEAN
EFIAPI
RootTestFn (
  IN DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // Check default values as per 2.3.5 of DT spec,
  // as test root node has no #address-cells or #size-cells.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);

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
TestG0Fn (
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
TestG1Fn (
  IN DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->IsDmaCoherent);
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
TestG2Fn (
  IN DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->AddressCells == 4);
  ASSERT (DtIo->SizeCells == 3);
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
TestG2P0Fn (
  IN DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // These should be inherited from g2.
  //
  ASSERT (DtIo->AddressCells == 4);
  ASSERT (DtIo->SizeCells == 3);
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
TestG2P0C1Fn  (
  IN DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // These should be the defaults, as Parent1 doesn't
  // have own #address-cells and #size-cells and there is
  // no inheritance.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);
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
TestG2P1Fn (
  IN DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // These should be not inherited from g2.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 2);
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
  //
  // Test handling for default #address-cells and #size-cells.
  //

  NODE_TEST (Buffer, RootTestFn);

  //
  // Test compatible and dma-coherent properties.
  //

  NEW_NODE (
    Buffer,
    g0,
    TestG0Fn,
    fdt_property_string (Buffer, "compatible", "test1_compatible")
    );

  //
  // Test dma-coherent property.
  //

  NEW_NODE (
    Buffer,
    g1,
    TestG1Fn,
    fdt_property (Buffer, "dma-coherent", NULL, 0)
    );

  //
  // More tests for #address-cells and #size-cells.
  //

  BEGIN_NODE (Buffer, g2);
  NODE_TEST (Buffer, TestG2Fn);
  fdt_property_u32 (Buffer, "#address-cells", 4);
  fdt_property_u32 (Buffer, "#size-cells", 3);
  BEGIN_NODE (Buffer, g2p0);
  NODE_TEST (Buffer, TestG2P0Fn);
  BEGIN_NODE (Buffer, g2p0c1);
  NODE_TEST (Buffer, TestG2P0C1Fn);
  END_NODE (Buffer, g2p0c1);
  END_NODE (Buffer, g2p0);
  BEGIN_NODE (Buffer, g2p1);
  NODE_TEST (Buffer, TestG2P1Fn);
  fdt_property_u32 (Buffer, "#address-cells", 2);
  fdt_property_u32 (Buffer, "#size-cells", 2);
  END_NODE (Buffer, g2p1);
  END_NODE (Buffer, g2);

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
