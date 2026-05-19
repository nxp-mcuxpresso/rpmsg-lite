# 06_rpmsg_adapter — Developer Notes

## Overview

`06_rpmsg_adapter` tests the `HAL_Rpmsg*` adapter layer (`mcuxsdk/components/rpmsg/fsl_adapter_rpmsg.c`) across **7 test cases** covering the full init/deinit/reinit lifecycle, bidirectional send/receive, zero-copy send, callback replacement, low-power stubs, and multi-endpoint operation.

The test is designed to run on two cores simultaneously:

| Role | CMake target | `HAL_RPMSG_SELECT_ROLE` |
|------|-------------|------------------------|
| Primary (master) | `primary/` | `0U` |
| Secondary (remote) | `secondary/` | `1U` |

### Test case order (both cores must match exactly)

```
tc_6  ep_ready_event   — regression: READY event must fire before first HAL_RpmsgSend
tc_1  init_deinit      — basic init / deinit lifecycle
tc_2  send_receive     — bidirectional copy-send, 2 messages each way
tc_3  nocopy_send      — zero-copy send path
tc_4  rx_callback      — runtime callback replacement
tc_5  lowpower_stubs   — HAL_RpmsgEnterLowpower / ExitLowpower (return error, no HW)
tc_7  multi_ep         — MAX_EP_COUNT endpoints in parallel
```

Each test case calls `HAL_RpmsgMcmgrInit()` / `HAL_RpmsgInit()` / `HAL_RpmsgDeinit()` in sequence, so the adapter is fully torn down and re-initialised **between every test case** — 7 full init/deinit/reinit cycles total.

---

## Supported boards

| Board | Core pair | Transport | Notes |
|-------|-----------|-----------|-------|
| `evkmimxrt1180` | cm33 + cm7 | MU (memory-mapped) | Standard MU, shared OCRAM |
| `lpcxpresso55s69` | cm33 + cm33 | MU | Standard MU |
| `evkmimxrt1160` | cm7 + cm4 | MU | Standard MU |
| `evkbmimxrt1170` | cm7 + cm4 | MU | Standard MU |
| `kw47evk` | cm33_core0 + cm33_core1 (NBU) | IMU | Wireless; NBU ~64 MHz |
| `mcxw72evk` | cm33_core0 + cm33_core1 (NBU) | IMU | Wireless; NBU ~64 MHz |
| `frdmmcxw72` | cm33_core0 + cm33_core1 (NBU) | IMU | Wireless; NBU ~64 MHz |

---

## Architecture: the COREUP/READY handshake

The original `fsl_adapter_rpmsg.c` used a simple one-way handshake: primary waited until secondary fired a READY event, then called `rpmsg_lite_master_init()`. This worked reliably for single-init scenarios but had a race on **reinit cycles** (every test case after tc_6 is a reinit):

- On deinit, primary clears `ready_seen`.
- If secondary fires READY *before* primary re-enters its wait loop, `ready_seen` is set before the clear — primary spins forever waiting for a READY that already happened.

The current implementation uses a **two-way COREUP/READY gate** to eliminate this race entirely:

```
PRIMARY                                   SECONDARY
───────                                   ─────────
HAL_RpmsgMcmgrInit()
  clear ready_seen
  loop:
    send COREUP ping ──────────────────►  receives COREUP
    wait ~100k iters                      (knows primary is in wait loop)
    if READY received → break             rpmsg_lite_remote_init()
    else re-ping                          ISR now registered
                        ◄─────────────── send READY
  ready_seen = 1 → exits loop
  [optional IMU delay]
  rpmsg_lite_master_init()
    virtqueue kick ───────────────────►  IMU ISR → link_up = true
                                          exit is_link_up() spin
                                          s_rpmsgEptCount = 0
  [RPMSG_MASTER_INIT_DELAY_MS sleep]     HAL_RpmsgInit()
  s_rpmsgEptCount = 0                      create endpoint
  HAL_RpmsgInit()                          send EP_READY ──────────────────►
    create endpoint                                         primary receives EP_READY
    rpmsg_lite_peer_ept_is_ready = 1 ◄──
  HAL_RpmsgSend() (peer ready)
```

**Why COREUP is required (not just READY):**

Secondary cannot fire READY until *after* it receives COREUP, which primary only sends *after* clearing `ready_seen`. This gives a strict ordering guarantee: when primary exits its COREUP ping loop having seen READY, it is impossible for that READY to be a stale event from the previous cycle.

---

## Key design decisions vs. the original code

### 1. One-shot flags: `s_rpmsg_init_global`, `s_mcmgr_hw_init_done`, `s_mcmgr_remote_startup_done`

| Flag | Where | What it guards | Why not reset on deinit |
|------|-------|----------------|------------------------|
| `s_rpmsg_init_global` | both cores | `LIST_Init` + `MCMGR_RegisterEvent` | Re-registering the event handler on an already-live MCMGR link corrupts the handler table |
| `s_mcmgr_hw_init_done` | primary | `MCMGR_Init` + `MCMGR_StartCore` | Remote core is still running; a second `StartCore` jams the MU TX register |
| `s_mcmgr_remote_startup_done` | secondary | `MCMGR_GetStartupData` | On reinit cycles `coreContext.state == kMCMGR_RunningCoreState`; `GetStartupData` returns `kStatus_MCMGR_NotReady` and would spin forever |

### 2. ISR callback must NOT call `HAL_RpmsgSend` / `HAL_RpmsgNoCopySend`

On IMU/wireless boards the TX path (`rpmsg_lite_send`) spins waiting for a free TX slot in the IMU FIFO. If called from inside the RX ISR callback, the TX spin cannot be serviced while the RX interrupt is still active — **deadlock**.

**Fix applied in `secondary/main.c`:** All RX callbacks only store the received payload and set a flag. The actual reply send is done from the test body (main-thread context) after `wait_flag()` returns:

```c
// WRONG — deadlocks on IMU boards:
static hal_rpmsg_return_status_t bad_callback(void *param, uint8_t *data, uint32_t len)
{
    HAL_RpmsgSend(handle, reply, sizeof(reply));  // ← spins in ISR!
    return kStatus_HAL_RL_RELEASE;
}

// CORRECT — defer to main thread:
static hal_rpmsg_return_status_t good_callback(void *param, uint8_t *data, uint32_t len)
{
    memcpy(&s_rxData, data, len);
    s_rxReceived = 1U;            // ← just set a flag
    return kStatus_HAL_RL_RELEASE;
}
// ... in test body:
wait_flag(&s_rxReceived, 1U, TC_WAIT_RETRY_COUNT);
HAL_RpmsgSend(handle, reply, sizeof(reply));  // ← safe: main thread
```

This pattern applies to **both** `data_rx_callback` (tc_2, tc_4) and `nocopy_rx_callback` (tc_3) and `multi_rx_callback` (tc_7).

### 3. `RPMSG_MASTER_INIT_DELAY_MS` — position matters

This compile-time knob inserts a `env_sleep_msec()` call on the **primary** side. It must be placed **after** `rpmsg_lite_master_init()` returns, not before.

**Why after, not before:**

```
rpmsg_lite_master_init()  →  sends virtqueue kick to NBU via IMU
  returns immediately
                              NBU receives kick (ISR)
                              exits rpmsg_lite_is_link_up() spin
                              completes HAL_RpmsgMcmgrRemoteInit()
                              calls HAL_RpmsgInit() → creates endpoint
                              sends EP_READY to primary

env_sleep_msec(5)         ←  PRIMARY SLEEPS HERE (gives NBU time above)

s_rpmsgEptCount = 0
HAL_RpmsgInit()
  rpmsg_lite_peer_ept_is_ready = 1  (EP_READY already received)
HAL_RpmsgSend() → endpoint exists on NBU → callback fires → PASS
```

Without the delay, primary races through `s_rpmsgEptCount = 0` → `HAL_RpmsgInit()` → fires EP_READY → calls `HAL_RpmsgSend()` before the NBU endpoint even exists. The first message is dropped and tc_2 fails with "Timeout waiting for rx_callback".

**Sleeping before `rpmsg_lite_master_init()` does nothing:** the NBU is still in its `rpmsg_lite_is_link_up()` spin waiting for the kick — it cannot make progress until the kick arrives.

**Default value:** `0U` (macro defined in `fsl_adapter_rpmsg.c`). The `#if` guard compiles the sleep away entirely on all boards that don't set it:

```c
#ifndef RPMSG_MASTER_INIT_DELAY_MS
#define RPMSG_MASTER_INIT_DELAY_MS 0U   // zero = no-op; #if compiled away
#endif
...
#if (RPMSG_MASTER_INIT_DELAY_MS > 0U)
    env_sleep_msec(RPMSG_MASTER_INIT_DELAY_MS);
#endif
```

**Impact on automotive applications:** None. Automotive kw47 apps never define `RPMSG_MASTER_INIT_DELAY_MS`. Their application startup code (peripheral init, RTOS task creation, etc.) naturally takes far longer than 5 ms between `HAL_RpmsgMcmgrInit()` and the first `HAL_RpmsgSend()`.

### 4. `rpmsg_lite_peer_ept_is_ready` reset on every reinit

`HAL_RpmsgInit()` now explicitly clears `rpmsg_lite_peer_ept_is_ready = 0U` before proceeding. Without this, the flag left over from the previous test cycle would make `HAL_RpmsgSendTimeout()`'s spin-wait skip immediately — sending before the peer has re-created its endpoint.

### 5. `s_rpmsgPeerEptStat` list cleanup on `HAL_RpmsgDeinit`

The original code left stale `next`/`prev` pointers in the embedded `list_element_t` after `LIST_RemoveElement`. On the next `LIST_AddTail` call the stale pointers corrupt the list. The fix zeroes the `list_element_t` struct after removal:

```c
(void)LIST_RemoveElement((list_element_handle_t)&s_rpmsgPeerEptStat[i]);
(void)memset(&s_rpmsgPeerEptStat[i].link, 0, sizeof(s_rpmsgPeerEptStat[i].link));
s_rpmsgPeerEptStat[i].rpmsgHandle = NULL;
```

---

## Board-specific configuration (reconfig.cmake knobs)

All IMU/wireless boards set these in `tests/_boards/<board>/06_rpmsg_adapter/reconfig.cmake`:

| Define | Typical value | Purpose |
|--------|--------------|---------|
| `REMOTE_CORE_BOOT_ADDRESS` | `0x48800000U` | Flash address where NBU firmware is placed |
| `RPMSG_REMOTE_READY_RETRY_COUNT` | `30000000U` | Iterations for the COREUP ping timeout (~3× default); NBU boot takes longer than CM33 |
| `TC_WAIT_RETRY_COUNT` | `50000000U` | Iterations for `wait_flag()` in secondary tests (~2.3 s at 64 MHz); longer than default because NBU is slower |
| `RPMSG_MASTER_INIT_DELAY_MS` | `5U` | ms to sleep after `rpmsg_lite_master_init()` before setting `s_rpmsgEptCount = 0` |
| `SH_MEM_NOT_TAKEN_FROM_LINKER` | (flag) | Secondary cm33_core1: shared memory address not in linker script; use fixed addresses below |

For IMU secondary core only (linker symbol injection):

```cmake
# armgcc
mcux_add_linker_symbol(
    SYMBOLS "rpmsg_sh_mem_start=0xB0008800 rpmsg_sh_mem_end=0xB000A000"
)
# IAR
mcux_add_configuration(
    LD "--define_symbol rpmsg_sh_mem_start=0xB0008800 --define_symbol rpmsg_sh_mem_end=0xB000A000"
)
```

The NBU sees shared memory at `0xB0008800` (CPU2 alias); primary sees the same physical RAM at `0x489c8800` (CPU1 alias). Both are passed at CMake/linker time — no runtime address negotiation needed.

---

## Adding a new board

1. **Create `tests/_boards/<board>/06_rpmsg_adapter/reconfig.cmake`**
   - For standard MU boards: usually empty or minimal (default values suffice)
   - For IMU/wireless boards: copy from `tests/_boards/kw47evk/06_rpmsg_adapter/reconfig.cmake` and adjust shmem addresses and boot address for the new SoC

2. **Add entries to `primary/example.yml`**
   ```yaml
   - name: test_06_rpmsg_adapter_primary_core@cm33_core0
     board: <board>@cm33_core0
     ...
   ```

3. **Add entries to `secondary/example.yml`**
   ```yaml
   - name: test_06_rpmsg_adapter_secondary_core@cm33_core1
     board: <board>@cm33_core1
     ...
   ```

4. **For IMU boards:** verify NBU shmem addresses match the SoC memory map (check `fsl_device_registers.h` or the reference manual for the CPU2 alias base address)

5. **Run the test** — if tc_2 fails with "Timeout waiting for rx_callback", increase `RPMSG_MASTER_INIT_DELAY_MS` (try 10–20 ms). If `HAL_RpmsgMcmgrInit` returns timeout on secondary, increase `RPMSG_REMOTE_READY_RETRY_COUNT`.

---

## File map

```
tests/06_rpmsg_adapter/
├── DEVELOPER_NOTES.md          ← this file
├── primary/
│   ├── main.c                  ← primary test cases + runner
│   ├── CMakeLists.txt
│   └── example.yml             ← board matrix for primary
├── secondary/
│   ├── main.c                  ← secondary (mirror) test cases + runner
│   ├── CMakeLists.txt
│   └── example.yml             ← board matrix for secondary
└── _boards/                    (under tests/_boards/)
    ├── kw47evk/06_rpmsg_adapter/reconfig.cmake
    ├── kw47loc/06_rpmsg_adapter/reconfig.cmake   ← local debug board
    ├── mcxw72evk/06_rpmsg_adapter/reconfig.cmake
    ├── frdmmcxw72/06_rpmsg_adapter/reconfig.cmake
    └── evkmimxrt1180/{cm33,cm7}/reconfig.cmake

mcuxsdk/components/rpmsg/
└── fsl_adapter_rpmsg.c         ← the adapter under test
```

---

## Frequently asked questions

**Q: Why does `HAL_RpmsgMcmgrInit` fail on reinit cycles with the original code?**

The original code called `MCMGR_Init()` + `MCMGR_StartCore()` on every `HAL_RpmsgMcmgrInit()` call. On reinit cycles the remote core is already running; a second `MCMGR_StartCore()` writes to the MU TX register while it is still active, corrupting the transport. The fix: `s_mcmgr_hw_init_done` gates these calls to first-boot only.

**Q: Why is `MCMGR_GetStartupData` skipped on secondary reinit cycles?**

`MCMGR_GetStartupData` reads the startup value written by `MCMGR_StartCore`. On reinit cycles `StartCore` is not called again (see above), so no new startup data was written. The MCMGR core-context state is `kMCMGR_RunningCoreState` (not `kMCMGR_ResetCoreState`), so `GetStartupData` returns `kStatus_MCMGR_NotReady` and the spin-wait would never exit. The fix: `s_mcmgr_remote_startup_done` skips it after first boot.

**Q: Is the 5 ms delay safe for production (automotive) use?**

Yes — automotive application code never defines `RPMSG_MASTER_INIT_DELAY_MS`, so the `#if (RPMSG_MASTER_INIT_DELAY_MS > 0U)` block is compiled away entirely. The delay exists only in the test binary, set via `reconfig.cmake`. There is zero overhead in production firmware.

**Q: Why `RL_BUFFER_COUNT=2` on wireless boards?**

The IMU FIFO on NBU has limited depth. With `RL_BUFFER_COUNT=2` (one per vring direction) the virtqueue descriptors fit comfortably in the shared memory region and the FIFO never backs up. Larger values would require a larger shmem region and longer IMU drain times.
