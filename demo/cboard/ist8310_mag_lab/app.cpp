#include "ist8310_lab_protocol.hpp"
#include "bsp_indicator.hpp"
#include "cboard_demo_debug.hpp"
#include "stm32f4xx.h"
#include "tx_api.h"

#include <cstdint>

extern "C" {
volatile std::uint32_t pnx_ist8310_lab_state = 0U;
volatile std::uint32_t pnx_ist8310_lab_fault = 0U;
volatile std::uint32_t pnx_ist8310_device_id = 0U;
volatile std::uint32_t pnx_ist8310_status = 0U;
volatile std::int32_t pnx_ist8310_x = 0;
volatile std::int32_t pnx_ist8310_y = 0;
volatile std::int32_t pnx_ist8310_z = 0;
volatile std::uint32_t pnx_ist8310_sample_count = 0U;
volatile std::uint32_t pnx_ist8310_change_count = 0U;
volatile std::uint32_t pnx_ist8310_i2c_error = 0U;
}

namespace
{

using cboard::ist8310_lab::raw_sample;

TX_THREAD mag_thread{};
alignas(8) ULONG mag_stack[384]{};
CHAR mag_name[] = "cboard ist8310 lab";
constexpr std::uint16_t device_address =
    static_cast<std::uint16_t>(
        cboard::ist8310_lab::i2c_address_7bit << 1U);
constexpr ULONG sample_ticks =
    TX_TIMER_TICKS_PER_SECOND / 50U;
constexpr std::uint32_t fault_init = 1U;
constexpr std::uint32_t fault_identity = 2U;
constexpr std::uint32_t fault_transfer = 4U;
constexpr std::uint32_t i2c_wait_limit = 500000U;

void init_gpio() noexcept
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_GPIOCEN |
                    RCC_AHB1ENR_GPIOGEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C3EN;
    (void)RCC->APB1ENR;

    GPIOA->MODER =
        (GPIOA->MODER & ~(3U << (8U * 2U))) |
        (2U << (8U * 2U));
    GPIOC->MODER =
        (GPIOC->MODER & ~(3U << (9U * 2U))) |
        (2U << (9U * 2U));
    GPIOA->OTYPER |= 1U << 8U;
    GPIOC->OTYPER |= 1U << 9U;
    GPIOA->OSPEEDR |= 3U << (8U * 2U);
    GPIOC->OSPEEDR |= 3U << (9U * 2U);
    GPIOA->PUPDR &= ~(3U << (8U * 2U));
    GPIOC->PUPDR &= ~(3U << (9U * 2U));
    GPIOA->AFR[1] =
        (GPIOA->AFR[1] & ~(0xFU << 0U)) |
        (4U << 0U);
    GPIOC->AFR[1] =
        (GPIOC->AFR[1] & ~(0xFU << 4U)) |
        (4U << 4U);
}

void reset_sensor()
{
    GPIOG->BSRR = 1U << (6U + 16U);
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 100U);
    GPIOG->BSRR = 1U << 6U;
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 20U);
}

bool wait_sr1(std::uint32_t mask) noexcept
{
    std::uint32_t timeout = i2c_wait_limit;
    while ((I2C3->SR1 & mask) == 0U && timeout-- != 0U)
    {
    }
    if (timeout == 0U)
    {
        pnx_ist8310_i2c_error = I2C3->SR1;
        I2C3->CR1 |= I2C_CR1_STOP;
        return false;
    }
    return true;
}

bool wait_bus_idle() noexcept
{
    std::uint32_t timeout = i2c_wait_limit;
    while ((I2C3->SR2 & I2C_SR2_BUSY) != 0U && timeout-- != 0U)
    {
    }
    return timeout != 0U;
}

void clear_addr() noexcept
{
    const volatile std::uint32_t sr1 = I2C3->SR1;
    const volatile std::uint32_t sr2 = I2C3->SR2;
    (void)sr1;
    (void)sr2;
}

bool begin_write() noexcept
{
    if (!wait_bus_idle())
    {
        return false;
    }
    I2C3->CR1 |= I2C_CR1_START;
    if (!wait_sr1(I2C_SR1_SB))
    {
        return false;
    }
    I2C3->DR = device_address;
    if (!wait_sr1(I2C_SR1_ADDR))
    {
        return false;
    }
    clear_addr();
    return true;
}

bool write_reg(std::uint8_t reg, std::uint8_t value) noexcept
{
    if (!begin_write() || !wait_sr1(I2C_SR1_TXE))
    {
        return false;
    }
    I2C3->DR = reg;
    if (!wait_sr1(I2C_SR1_TXE))
    {
        return false;
    }
    I2C3->DR = value;
    if (!wait_sr1(I2C_SR1_BTF))
    {
        return false;
    }
    I2C3->CR1 |= I2C_CR1_STOP;
    return true;
}

bool read_reg(std::uint8_t reg, std::uint8_t& data) noexcept
{
    if (!begin_write() || !wait_sr1(I2C_SR1_TXE))
    {
        return false;
    }
    I2C3->DR = reg;
    if (!wait_sr1(I2C_SR1_BTF))
    {
        return false;
    }

    I2C3->CR1 |= I2C_CR1_START;
    if (!wait_sr1(I2C_SR1_SB))
    {
        return false;
    }
    I2C3->DR = device_address | 1U;
    if (!wait_sr1(I2C_SR1_ADDR))
    {
        return false;
    }
    I2C3->CR1 &= ~I2C_CR1_ACK;
    clear_addr();
    I2C3->CR1 |= I2C_CR1_STOP;
    if (!wait_sr1(I2C_SR1_RXNE))
    {
        return false;
    }
    data = static_cast<std::uint8_t>(I2C3->DR);
    I2C3->CR1 |= I2C_CR1_ACK;
    return true;
}

bool init_i2c3() noexcept
{
    init_gpio();
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C3RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C3RST;

    I2C3->CR1 = I2C_CR1_SWRST;
    I2C3->CR1 = 0U;
    I2C3->CR2 = 42U;
    I2C3->CCR = I2C_CCR_FS | 35U;
    I2C3->TRISE = 13U;
    I2C3->CR1 = I2C_CR1_ACK | I2C_CR1_PE;
    return true;
}

bool read_sample(raw_sample& sample) noexcept
{
    if (!write_reg(0x0AU, 0x01U))
    {
        return false;
    }
    tx_thread_sleep(sample_ticks);

    std::uint8_t status = 0U;
    std::uint8_t data[6]{};
    if (!read_reg(0x02U, status))
    {
        return false;
    }
    for (std::uint8_t index = 0U; index < 6U; ++index)
    {
        if (!read_reg(static_cast<std::uint8_t>(0x03U + index),
                      data[index]))
        {
            return false;
        }
    }
    pnx_ist8310_status = status;
    sample = {
        cboard::ist8310_lab::decode_i16(data[0], data[1]),
        cboard::ist8310_lab::decode_i16(data[2], data[3]),
        cboard::ist8310_lab::decode_i16(data[4], data[5]),
    };
    return true;
}

void mag_entry(ULONG)
{
    demo_debug_instance.threadx_started = 1U;
    cboard::demo::sync_system_diagnostics();
    (void)bsp::indicator::init();
    reset_sensor();

    if (!init_i2c3())
    {
        pnx_ist8310_lab_fault |= fault_init;
    }

    std::uint8_t id = 0U;
    if (!read_reg(0x00U, id))
    {
        pnx_ist8310_lab_fault |= fault_transfer;
    }
    pnx_ist8310_device_id = id;
    if (!cboard::ist8310_lab::identity_valid(id))
    {
        pnx_ist8310_lab_fault |= fault_identity;
    }

    if (!write_reg(0x41U, 0x24U) ||
        !write_reg(0x42U, 0xC0U))
    {
        pnx_ist8310_lab_fault |= fault_transfer;
    }
    pnx_ist8310_lab_state =
        pnx_ist8310_lab_fault == 0U ? 2U : 3U;

    raw_sample previous{};
    bool have_previous = false;
    for (;;)
    {
        raw_sample sample{};
        if (read_sample(sample))
        {
            if (have_previous &&
                cboard::ist8310_lab::sample_changed(
                    previous, sample, 32))
            {
                ++pnx_ist8310_change_count;
            }
            previous = sample;
            have_previous = true;
            pnx_ist8310_x = sample.x;
            pnx_ist8310_y = sample.y;
            pnx_ist8310_z = sample.z;
            ++pnx_ist8310_sample_count;
        }
        else
        {
            pnx_ist8310_lab_fault |= fault_transfer;
            pnx_ist8310_lab_state = 3U;
        }

        ++demo_debug_instance.heartbeat_count;
        demo_debug_instance.threadx_tick =
            static_cast<std::uint32_t>(tx_time_get());
        cboard::demo::sync_threadx(&mag_thread);
        if ((pnx_ist8310_sample_count % 25U) == 0U)
        {
            (void)bsp::indicator::toggle(
                bsp::indicator::channel::green);
        }
    }
}

} // namespace

extern "C" void app_start(void)
{
    demo_debug_instance.demo_kind =
        static_cast<std::uint32_t>(
            cboard::demo::kind::ist8310_mag_lab);
    const UINT status = tx_thread_create(
        &mag_thread, mag_name, mag_entry, 0U,
        mag_stack, sizeof(mag_stack), 10U, 10U,
        TX_NO_TIME_SLICE, TX_AUTO_START);
    demo_debug_instance.start_status = status;
    if (status != TX_SUCCESS)
    {
        pnx_ist8310_lab_state = 3U;
        pnx_ist8310_lab_fault = fault_init;
    }
}
