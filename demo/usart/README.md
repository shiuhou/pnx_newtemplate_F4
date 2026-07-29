# Legacy USART1 DMA Demo

這是從 `pnx_template` 保留的 USART1 DMA protocol 範例，不是本 F407
release candidate 的 application closure。`CMakeLists.txt` 不會選入
`demo/usart/usart_demo.cpp`，因此它沒有 F407 preset、燒錄流程或硬體驗證
狀態。

目前 F407 的 USART vertical slice 是 USART3/DBUS RX：

```text
PNX_ENABLE_DBUS_RX
→ pnx_bsp/usart
→ boards/dji_c_board_f407/pnx_backends/usart_backend.cpp
→ demo/cboard/dbus_rx
```

不要使用本目錄早期記錄的 template 專用建置、燒錄或 host-script 指令；
它們對本 repository 無效。若未來要啟用 USART1，應以獨立 vertical slice
定義 application、CubeMX/board ownership、測試與硬體驗證，而不是直接把
此 demo 加入 Core。
