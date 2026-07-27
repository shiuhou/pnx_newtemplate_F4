#include "bsp_indicator.hpp"
#include "cboard_demo_debug.hpp"
#include "pwm_servo_sequence.hpp"
#include "stm32f4xx.h"
#include "tx_api.h"

#include <cstdint>

extern "C" {
volatile std::uint32_t pnx_pwm_lab_arm_token = 0U;
volatile std::uint32_t pnx_pwm_lab_state = 0U;
volatile std::uint32_t pnx_pwm_lab_pulse_us = 0U;
volatile std::uint32_t pnx_pwm_lab_step_count = 0U;
volatile std::uint32_t pnx_pwm_lab_fault_count = 0U;
}

namespace
{

TX_THREAD pwm_thread{};
alignas(8) ULONG pwm_stack[384]{};
CHAR pwm_name[] = "cboard pwm servo lab";
cboard::pwm_servo_lab::sequence test_sequence{};

constexpr ULONG poll_ticks = TX_TIMER_TICKS_PER_SECOND / 50U;
constexpr ULONG center_ticks = TX_TIMER_TICKS_PER_SECOND;
constexpr ULONG step_ticks = TX_TIMER_TICKS_PER_SECOND / 2U;

void set_pulse(std::uint16_t pulse_us) noexcept
{
    TIM1->CCR2 = pulse_us;
    pnx_pwm_lab_pulse_us = pulse_us;
}

bool init_pwm_c2() noexcept
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;

    GPIOE->MODER =
        (GPIOE->MODER & ~(3U << (11U * 2U))) |
        (2U << (11U * 2U));
    GPIOE->OTYPER &= ~(1U << 11U);
    GPIOE->OSPEEDR =
        (GPIOE->OSPEEDR & ~(3U << (11U * 2U))) |
        (2U << (11U * 2U));
    GPIOE->PUPDR &= ~(3U << (11U * 2U));
    GPIOE->AFR[1] =
        (GPIOE->AFR[1] & ~(0xFU << ((11U - 8U) * 4U))) |
        (1U << ((11U - 8U) * 4U));

    TIM1->CR1 = 0U;
    TIM1->PSC = 168U - 1U;
    TIM1->ARR = 20000U - 1U;
    TIM1->CCR2 = 1500U;
    TIM1->CCMR1 =
        (TIM1->CCMR1 &
         ~(TIM_CCMR1_OC2M | TIM_CCMR1_CC2S)) |
        TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 |
        TIM_CCMR1_OC2PE;
    TIM1->CCER =
        (TIM1->CCER & ~(TIM_CCER_CC2P | TIM_CCER_CC2NP)) |
        TIM_CCER_CC2E;
    TIM1->BDTR |= TIM_BDTR_MOE;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
    return true;
}

void run_sequence() noexcept
{
    pnx_pwm_lab_state = 1U;
    while (!test_sequence.complete())
    {
        set_pulse(test_sequence.pulse_us());
        ++pnx_pwm_lab_step_count;
        const ULONG delay =
            pnx_pwm_lab_step_count == 1U ? center_ticks : step_ticks;
        tx_thread_sleep(delay);
        test_sequence.advance();
    }
    set_pulse(1500U);
    pnx_pwm_lab_state = 2U;
}

void pwm_entry(ULONG)
{
    demo_debug_instance.threadx_started = 1U;
    cboard::demo::sync_system_diagnostics();
    const types::status indicator_status = bsp::indicator::init();
    demo_debug_instance.indicator_status =
        static_cast<std::uint32_t>(indicator_status);
    if (indicator_status != types::status::ok)
    {
        ++pnx_pwm_lab_fault_count;
        pnx_pwm_lab_state = 3U;
    }

    for (;;)
    {
        const std::uint32_t token = pnx_pwm_lab_arm_token;
        if (token != 0U)
        {
            pnx_pwm_lab_arm_token = 0U;
            if (test_sequence.arm(token) && init_pwm_c2())
            {
                run_sequence();
            }
            else
            {
                ++pnx_pwm_lab_fault_count;
                pnx_pwm_lab_state = 3U;
            }
        }

        ++demo_debug_instance.heartbeat_count;
        demo_debug_instance.threadx_tick =
            static_cast<std::uint32_t>(tx_time_get());
        cboard::demo::sync_threadx(&pwm_thread);
        (void)bsp::indicator::toggle(
            bsp::indicator::channel::green);
        tx_thread_sleep(poll_ticks);
    }
}

} // namespace

extern "C" void app_start(void)
{
    demo_debug_instance.demo_kind =
        static_cast<std::uint32_t>(
            cboard::demo::kind::pwm_servo_lab);
    const UINT status =
        tx_thread_create(&pwm_thread, pwm_name, pwm_entry, 0U,
                         pwm_stack, sizeof(pwm_stack), 10U, 10U,
                         TX_NO_TIME_SLICE, TX_AUTO_START);
    demo_debug_instance.start_status = status;
    if (status != TX_SUCCESS)
    {
        ++pnx_pwm_lab_fault_count;
        pnx_pwm_lab_state = 3U;
    }
}
