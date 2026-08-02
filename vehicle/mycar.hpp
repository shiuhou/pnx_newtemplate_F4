#pragma once

namespace vehicle::mycar
{

// 由 demo/app.cpp 在 PNX_APP_MYCAR_CHASSIS closure 中呼叫一次。
// 重複啟動防護由 chassis runtime 負責。
void run() noexcept;

} // namespace vehicle::mycar
