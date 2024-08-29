/** @file
    EFI Devicetree Interrupt Protocol provides a mechanism to register
    interrupt handlers, and is implemented by interrupt controller
    drivers.

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DT_INTERRUPT_H__
#define __DT_INTERRUPT_H__

#include <Protocol/DtIo.h>
#include <Protocol/DebugSupport.h>

#define EFI_DT_INTERRUPT_PROTOCOL_GUID \
  { \
    0x5ce5a2b0, 0x2838, 0x3c35, {0x1e, 0xe3, 0x42, 0x5e, 0x36, 0x50, 0xa3, 0x9c } \
  }

typedef struct _EFI_DT_INTERRUPT_PROTOCOL  EFI_DT_INTERRUPT_PROTOCOL;
typedef VOID                               *EFI_DT_INTERRUPT_COOKIE;

/**
  C Interrupt Handler called in the interrupt context when Cookie interrupt is active.

  @param Cookie         Identifies the interrupt registered.
  @param CookieContext  Additional context passed during registration.
  @param SystemContext  Pointer to system register context. Mostly used by debuggers and will
                        update the system context after the return from the interrupt if
                        modified. Don't change these values unless you know what you are doing.

**/
typedef
VOID
(EFIAPI *EFI_DT_INTERRUPT_HANDLER)(
  IN  EFI_DT_INTERRUPT_COOKIE Cookie,
  IN  VOID                    *CookieContext,
  IN  EFI_SYSTEM_CONTEXT      SystemContext
  );

/**
  Register Handler for the specified interrupt source.

  Interrupts->Iter is only modified on success.

  @param This            Instance pointer for this protocol.
  @param InterruptData   Interrupt specifier.
  @param Handler         Callback for interrupt.
  @param CookieContext   Additional context to pass to Handler.
  @param Cookie          A unique value used for further operations on a registered interrupt.

  @retval EFI_SUCCESS            Interrupt handler registered, cookie returned.
  @retval EFI_UNSUPPORTED        Configuration not supported.
  @retval EFI_INVALID_PARAMETER  Bad parameter.
  @retval EFI_DEVICE_ERROR       Hardware could not be programmed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_INTERRUPT_REGISTER)(
  IN  EFI_DT_INTERRUPT_PROTOCOL *This,
  IN  EFI_DT_PROPERTY           *InterruptData,
  IN  EFI_DT_INTERRUPT_HANDLER  Handler,
  IN  VOID                      *CookieContext,
  OUT EFI_DT_INTERRUPT_COOKIE   *Cookie
  );

/**
  Unregister handler for an interrupt.

  @param This    Instance pointer for this protocol.
  @param Cookie  As provided by EFI_DT_INTERRUPT_REGISTER callback.

  @retval EFI_SUCCESS            Interrupt unregistered.
  @retval EFI_INVALID_PARAMETER  Bad parameter.
  @retval EFI_DEVICE_ERROR       Hardware could not be programmed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_INTERRUPT_UNREGISTER)(
  IN  EFI_DT_INTERRUPT_PROTOCOL *This,
  IN  EFI_DT_INTERRUPT_COOKIE   Cookie
  );

/**
  Enable interrupt.

  @param This    Instance pointer for this protocol.
  @param Cookie  As provided by EFI_DT_INTERRUPT_REGISTER callback.

  @retval EFI_SUCCESS            Interrupt enabled.
  @retval EFI_UNSUPPORTED        Configuration not supported.
  @retval EFI_INVALID_PARAMETER  Bad parameter.
  @retval EFI_DEVICE_ERROR       Hardware could not be programmed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_INTERRUPT_ENABLE)(
  IN  EFI_DT_INTERRUPT_PROTOCOL *This,
  IN  EFI_DT_INTERRUPT_COOKIE   Cookie
  );

/**
  Disable interrupt.

  @param This     Instance pointer for this protocol
  @param Cookie   As provided by EFI_DT_INTERRUPT_REGISTER callback.

  @retval EFI_SUCCESS            Interrupt disabled.
  @retval EFI_UNSUPPORTED        Configuration not supported.
  @retval EFI_INVALID_PARAMETER  Bad parameter.
  @retval EFI_DEVICE_ERROR       Hardware could not be programmed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DT_INTERRUPT_DISABLE)(
  IN  EFI_DT_INTERRUPT_PROTOCOL *This,
  IN  EFI_DT_INTERRUPT_COOKIE   Cookie
  );

struct _EFI_DT_INTERRUPT_PROTOCOL {
  EFI_DT_INTERRUPT_REGISTER      RegisterInterrupt;
  EFI_DT_INTERRUPT_UNREGISTER    UnregisterInterrupt;
  EFI_DT_INTERRUPT_ENABLE        EnableInterrupt;
  EFI_DT_INTERRUPT_DISABLE       DisableInterrupt;
};

extern EFI_GUID  gEfiDtInterruptProtocolGuid;

#endif /* __DT_INTERRUPT_H__ */
