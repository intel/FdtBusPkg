# EFI Devicetree Interrupt Protocol

> [!NOTE]
> See [FdtBusPkg Documentation Style and Terms Definitions](StyleAndTerms.md) first.

This section provides a detailed description of the `EFI_DT_INTERRUPT_PROTOCOL`.
This protocol is used by DT controller drivers for interrupt controllers.

The interfaces provided in the `EFI_DT_INTERRUPT_PROTOCOL` are for registering
an interrupt handler and enabling and disabling the registered interrupt source. The
protocol is specifically tailored to an environment, which relies on describing
the platform devices using a Devicetree, including the timer hardware exposed to UEFI via
the architectural timer protocol.

## EFI_DT_INTERRUPT_PROTOCOL

### Summary

Provides an interface for registering an interrupt handler and manipulating the
registered interrupt source. The inteface is meant to be used by a DT controller
driver (in practice, a timer source driver implementing the `EFI_TIMER_ARCH_PROTOCOL`).

### GUID

```
#define EFI_DT_INTERRUPT_PROTOCOL_GUID \
  { \
    0x5ce5a2b0, 0x2838, 0x3c35, {0x1e, 0xe3, 0x42, 0x5e, 0x36, 0x50, 0xa3, 0x9c } \
  }
```

### Protocol Interface Structure

```
typedef struct _EFI_DT_INTERRUPT_PROTOCOL {
  EFI_DT_INTERRUPT_REGISTER      RegisterInterrupt;
  EFI_DT_INTERRUPT_UNREGISTER    UnregisterInterrupt;
  EFI_DT_INTERRUPT_ENABLE        EnableInterrupt;
  EFI_DT_INTERRUPT_DISABLE       DisableInterrupt;
} EFI_DT_INTERRUPT_PROTOCOL;
```

### Members

| Name | Description |
| ---- | ----------- |
| [`RegisterInterrupt`](#efi_dt_interrupt_protocolregisterinterrupt) | Register an interrupt handler. |
| [`UnregisterInterrupt`](#efi_dt_interrupt_protocolunregisterinterrupt) | Unregisters an interrupt handler. |
| [`EnableInterrupt`](#efi_dt_interrupt_protocolenableinterrupt) | Unmask the interrupt with a registered interrupt handler. |
| [`DisableInterrupt`](#efi_dt_interrupt_protocoldisableinterrupt) | Mask the interrupt with a registered interrupt handler. |

### Related Definitions

```
typedef VOID *EFI_DT_INTERRUPT_COOKIE;

typedef
VOID
(EFIAPI *EFI_DT_INTERRUPT_HANDLER)(
  IN  EFI_DT_INTERRUPT_COOKIE Cookie,
  IN  VOID                    *CookieContext,
  IN  EFI_SYSTEM_CONTEXT      SystemContext
  );
```

### Description

The `EFI_DT_INTERRUPT_PROTOCOL` abstracts registered handlers with a `EFI_DT_INTERRUPT_COOKIE`. The cookie
uniquely identifies the registered interrupt and is used with further API calls to unregister, enable
and disable the interrupt. The actual interrupt configuration is entirely opaque, being specific
to the interrupt controller implementation itself, and is passed via a DT property blob.

### Interrupt Handler

Interrupt completion (aka EOI, end-of-interrupt) is done automatically after the handler completes.

### `EFI_DT_INTERRUPT_PROTOCOL.RegisterInterrupt()`

#### Description

Registers an interrupt handler for an interrupt source. On success, populates
`*Cookie` with a value uniquely identifying the interrupt source.

An `EFI_DT_INTERRUPT_PROTOCOL` implementation is only required to support one outstanding registered interrupt.

The `InterruptData` parameter is an `EFI_DT_PROPERTY` encoding an interrupt
specifier valid in the context of the interrupt controller driver. Such
an interrupt specifier could come from the _interrupts_ property for a DT
controller, or from the _interrupt-map_ of an interrupt nexus Devicetree node.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_INTERRUPT_REGISTER)(
  IN  EFI_DT_INTERRUPT_PROTOCOL *This,
  IN  EFI_DT_PROPERTY           *InterruptData,
  IN  EFI_DT_INTERRUPT_HANDLER  Handler,
  IN  VOID                      *CookieContext,
  OUT EFI_DT_INTERRUPT_COOKIE   *Cookie
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` |  Instance pointer for this protocol. |
| `InterruptData` | Interrupt specifier. |
| `Handler` | Callback for interrupt. |
| `CookieContext` | Additional context to pass to `Handler`. |
| `Cookie` | A unique value used for further operations on a registered interrupt. |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Interrupt handler registered and cookie returned. |
| `EFI_UNSUPPORTED` | Configuration not supported. |
| `EFI_INVALID_PARAMETER` | Bad parameter. |
| `EFI_DEVICE_ERROR` | Hardware could not be programmed. |

### `EFI_DT_INTERRUPT_PROTOCOL.UnregisterInterrupt()`

#### Description

Unregister handler for an interrupt.

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_INTERRUPT_UNREGISTER)(
  IN  EFI_DT_INTERRUPT_PROTOCOL *This,
  IN  EFI_DT_INTERRUPT_COOKIE   Cookie
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` |  Instance pointer for this protocol. |
| `Cookie` | As provided by [`RegisterInterrupt`](#efi_dt_interrupt_protocolregisterinterrupt). |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Interrupt unregistered. |
| `EFI_INVALID_PARAMETER` | Bad parameter. |
| `EFI_DEVICE_ERROR` | Hardware could not be programmed. |

### `EFI_DT_INTERRUPT_PROTOCOL.EnableInterrupt()`

#### Description

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_INTERRUPT_ENABLE)(
  IN  EFI_DT_INTERRUPT_PROTOCOL *This,
  IN  EFI_DT_INTERRUPT_COOKIE   Cookie
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` |  Instance pointer for this protocol. |
| `Cookie` | As provided by [`RegisterInterrupt`](#efi_dt_interrupt_protocolregisterinterrupt). |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Interrupt enabled. |
| `EFI_INVALID_PARAMETER` | Bad parameter. |
| `EFI_DEVICE_ERROR` | Hardware could not be programmed. |

### `EFI_DT_INTERRUPT_PROTOCOL.DisableInterrupt()`

#### Description

#### Prototype

```
typedef
EFI_STATUS
(EFIAPI *EFI_DT_INTERRUPT_DISABLE)(
  IN  EFI_DT_INTERRUPT_PROTOCOL *This,
  IN  EFI_DT_INTERRUPT_COOKIE   Cookie
  );
```

#### Parameters

| Parameter | Description |
| --------- | ----------- |
| `This` |  Instance pointer for this protocol. |
| `Cookie` | As provided by [`RegisterInterrupt`](#efi_dt_interrupt_protocolregisterinterrupt). |

#### Status Codes Returned

| Status Code | Description |
| ----------- | ----------- |
| `EFI_SUCCESS` | Interrupt disabled. |
| `EFI_INVALID_PARAMETER` | Bad parameter. |
| `EFI_DEVICE_ERROR` | Hardware could not be programmed. |
