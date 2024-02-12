/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
    Copyright (C) 2017 Andrei Evgenievich Warkentin

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FBP_APP_UTILS_LIB_H__
#define __FBP_APP_UTILS_LIB_H__

#include <Uefi.h>
#include <Protocol/DtIo.h>

typedef struct GET_OPT_CONTEXT {
  CHAR16    Opt;
  CHAR16    *OptArg;
  UINTN     OptIndex;
} GET_OPT_CONTEXT;

#define INIT_GET_OPT_CONTEXT(ContextPointer)  do {\
    (ContextPointer)->Opt = L'\0';               \
    (ContextPointer)->OptArg = NULL;             \
    (ContextPointer)->OptIndex = 1;              \
  } while (0)

EFI_STATUS
GetOpt (
  IN UINTN                Argc,
  IN CHAR16               **Argv,
  IN CHAR16               *OptionsWithArgs,
  IN OUT GET_OPT_CONTEXT  *Context
  );

EFI_STATUS
GetShellArgcArgv (
  IN  EFI_HANDLE  ImageHandle,
  OUT UINTN       *Argcp,
  OUT CHAR16      ***Argvp
  );

EFI_STATUS
FbpAppLookup (
  IN  CONST CHAR16        *String,
  OUT EFI_DT_IO_PROTOCOL  **OutDtIo,
  OUT EFI_HANDLE          *OutHandle OPTIONAL
  );

#endif /* __FBP_APP_UTILS_LIB_H__ */
