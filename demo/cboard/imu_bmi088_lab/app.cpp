#include "bmi088_lab_protocol.hpp"
#include "bsp_indicator.hpp"
#include "cboard_demo_debug.hpp"
#include "stm32f4xx.h"
#include "tx_api.h"

#include <cstddef>
#include <cstdint>

extern "C" {
volatile std::uint32_t pnx_bmi088_lab_state = 0U;
volatile std::uint32_t pnx_bmi088_lab_fault = 0U;
volatile std::uint32_t pnx_bmi088_accel_chip_id = 0U;
volatile std::uint32_t pnx_bmi088_gyro_chip_id = 0U;
volatile std::int32_t pnx_bmi088_ax = 0;
volatile std::int32_t pnx_bmi088_ay = 0;
volatile std::int32_t pnx_bmi088_az = 0;
volatile std::int32_t pnx_bmi088_gx = 0;
volatile std::int32_t pnx_bmi088_gy = 0;
volatile std::int32_t pnx_bmi088_gz = 0;
volatile std::uint32_t pnx_bmi088_sample_count = 0U;
volatile std::uint32_t pnx_bmi088_change_count = 0U;
volatile std::uint32_t pnx_bmi088_mode_scan[8] = {};
}

namespace
{

using cboard::bmi088_lab::raw_sample;

TX_THREAD imu_thread{};
alignas(8) ULONG imu_stack[384]{};
CHAR imu_name[] = "cboard bmi088 lab";

constexpr std::uint32_t fault_spi_timeout = 1U;
constexpr std::uint32_t fault_chip_id = 2U;
constexpr std::uint32_t spi_wait_limit = 200000U;
constexpr ULONG sample_ticks =
    TX_TIMER_TICKS_PER_SECOND / 100U;

void cs_set(bool accel, bool active) noexcept
{
    GPIO_TypeDef* const port = accel ? GPIOA : GPIOB;
    const std::uint32_t pin = accel ? (1U << 4U) : (1U << 0U);
    port->BSRR = active ? (pin << 16U) : pin;
}

bool spi_byte(std::uint8_t tx, std::uint8_t& rx) noexcept
{
    std::uint32_t timeout = spi_wait_limit;
    while ((SPI1->SR & SPI_SR_TXE) == 0U && timeout-- != 0U)
    {
    }
    if (timeout == 0U)
    {
        return false;
    }

    *reinterpret_cast<volatile std::uint8_t*>(&SPI1->DR) = tx;
    timeout = spi_wait_limit;
    while ((SPI1->SR & SPI_SR_RXNE) == 0U && timeout-- != 0U)
    {
    }
    if (timeout == 0U)
    {
        return false;
    }
    rx = *reinterpret_cast<volatile std::uint8_t*>(&SPI1->DR);
    return true;
}

bool wait_idle() noexcept
{
    std::uint32_t timeout = spi_wait_limit;
    while ((SPI1->SR & SPI_SR_BSY) != 0U && timeout-- != 0U)
    {
    }
    return timeout != 0U;
}

bool read_regs(
    bool accel, std::uint8_t address,
    std::uint8_t* data, std::size_t length) noexcept
{
    std::uint8_t discard = 0U;
    cs_set(accel, true);
    bool ok = spi_byte(static_cast<std::uint8_t>(address | 0x80U),
                       discard);
    if (ok && accel)
    {
        ok = spi_byte(0U, discard);
    }
    for (std::size_t i = 0; ok && i < length; ++i)
    {
        ok = spi_byte(0U, data[i]);
    }
    ok = wait_idle() && ok;
    cs_set(accel, false);
    return ok;
}

bool write_reg(
    bool accel, std::uint8_t address, std::uint8_t value) noexcept
{
    std::uint8_t discard = 0U;
    cs_set(accel, true);
    bool ok = spi_byte(static_cast<std::uint8_t>(address & 0x7FU),
                       discard);
    ok = ok && spi_byte(value, discard);
    ok = wait_idle() && ok;
    cs_set(accel, false);
    return ok;
}

void init_spi1() noexcept
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;

    GPIOA->BSRR = (1U << 4U);
    GPIOB->BSRR = (1U << 0U);

    GPIOA->MODER =
        (GPIOA->MODER & ~(3U << (7U * 2U))) |
        (2U << (7U * 2U));
    GPIOB->MODER =
        (GPIOB->MODER &
         ~((3U << (3U * 2U)) | (3U << (4U * 2U)))) |
        (2U << (3U * 2U)) | (2U << (4U * 2U));
    GPIOA->AFR[0] =
        (GPIOA->AFR[0] & ~(0xFU << (7U * 4U))) |
        (5U << (7U * 4U));
    GPIOB->AFR[0] =
        (GPIOB->AFR[0] &
         ~((0xFU << (3U * 4U)) | (0xFU << (4U * 4U)))) |
        (5U << (3U * 4U)) | (5U << (4U * 4U));
    GPIOA->OSPEEDR |= 3U << (7U * 2U);
    GPIOB->OSPEEDR |=
        (3U << (3U * 2U)) | (3U << (4U * 2U));
    GPIOA->PUPDR =
        (GPIOA->PUPDR & ~(3U << (7U * 2U))) |
        (1U << (7U * 2U));
    GPIOB->PUPDR =
        (GPIOB->PUPDR &
         ~((3U << (3U * 2U)) | (3U << (4U * 2U)))) |
        (1U << (3U * 2U)) | (1U << (4U * 2U));

    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                SPI_CR1_BR_1 | SPI_CR1_CPOL | SPI_CR1_CPHA;
    SPI1->CR2 = 0U;
    SPI1->CR1 |= SPI_CR1_SPE;
}

void scan_spi_modes() noexcept
{
    constexpr std::uint32_t base =
        SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_1;
    for (std::uint32_t mode = 0U; mode < 4U; ++mode)
    {
        SPI1->CR1 = base |
                    ((mode & 2U) != 0U ? SPI_CR1_CPOL : 0U) |
                    ((mode & 1U) != 0U ? SPI_CR1_CPHA : 0U);
        SPI1->CR1 |= SPI_CR1_SPE;
        std::uint8_t accel_id = 0U;
        std::uint8_t gyro_id = 0U;
        (void)read_regs(true, 0x00U, &accel_id, 1U);
        (void)read_regs(false, 0x00U, &gyro_id, 1U);
        pnx_bmi088_mode_scan[mode * 2U] = accel_id;
        pnx_bmi088_mode_scan[mode * 2U + 1U] = gyro_id;
        SPI1->CR1 &= ~SPI_CR1_SPE;
    }
    SPI1->CR1 = base | SPI_CR1_CPOL | SPI_CR1_CPHA |
                SPI_CR1_SPE;
}

bool configure_bmi088() noexcept
{
    std::uint8_t discard = 0U;
    cs_set(true, true);
    const bool accel_spi_mode = spi_byte(0U, discard) && wait_idle();
    cs_set(true, false);
    if (!accel_spi_mode)
    {
        return false;
    }

    if (!write_reg(true, 0x7EU, 0xB6U))
    {
        return false;
    }
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 10U);

    cs_set(true, true);
    const bool accel_spi_after_reset =
        spi_byte(0U, discard) && wait_idle();
    cs_set(true, false);
    if (!accel_spi_after_reset ||
        !write_reg(true, 0x7CU, 0x00U))
    {
        return false;
    }
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 100U);
    if (!write_reg(true, 0x7DU, 0x04U))
    {
        return false;
    }
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 20U);
    if (!write_reg(true, 0x41U, 0x01U) ||
        !write_reg(true, 0x40U, 0xABU))
    {
        return false;
    }

    if (!write_reg(false, 0x14U, 0xB6U))
    {
        return false;
    }
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 10U);
    return write_reg(false, 0x0FU, 0x01U) &&
           write_reg(false, 0x10U, 0x01U) &&
           write_reg(false, 0x11U, 0x00U);
}

bool read_sample(raw_sample& sample) noexcept
{
    std::uint8_t accel[6]{};
    std::uint8_t gyro[6]{};
    if (!read_regs(true, 0x12U, accel, sizeof(accel)) ||
        !read_regs(false, 0x02U, gyro, sizeof(gyro)))
    {
        return false;
    }
    sample = {
        cboard::bmi088_lab::decode_i16(accel[0], accel[1]),
        cboard::bmi088_lab::decode_i16(accel[2], accel[3]),
        cboard::bmi088_lab::decode_i16(accel[4], accel[5]),
        cboard::bmi088_lab::decode_i16(gyro[0], gyro[1]),
        cboard::bmi088_lab::decode_i16(gyro[2], gyro[3]),
        cboard::bmi088_lab::decode_i16(gyro[4], gyro[5]),
    };
    return true;
}

void publish(const raw_sample& sample) noexcept
{
    pnx_bmi088_ax = sample.ax;
    pnx_bmi088_ay = sample.ay;
    pnx_bmi088_az = sample.az;
    pnx_bmi088_gx = sample.gx;
    pnx_bmi088_gy = sample.gy;
    pnx_bmi088_gz = sample.gz;
}

void imu_entry(ULONG)
{
    demo_debug_instance.threadx_started = 1U;
    cboard::demo::sync_system_diagnostics();
    (void)bsp::indicator::init();
    init_spi1();
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 20U);
    scan_spi_modes();

    if (!configure_bmi088())
    {
        pnx_bmi088_lab_fault |= fault_spi_timeout;
        pnx_bmi088_lab_state = 3U;
    }

    std::uint8_t accel_id = 0U;
    std::uint8_t gyro_id = 0U;
    if (!read_regs(true, 0x00U, &accel_id, 1U) ||
        !read_regs(false, 0x00U, &gyro_id, 1U))
    {
        pnx_bmi088_lab_fault |= fault_spi_timeout;
    }
    pnx_bmi088_accel_chip_id = accel_id;
    pnx_bmi088_gyro_chip_id = gyro_id;
    if (!cboard::bmi088_lab::chip_ids_valid(accel_id, gyro_id))
    {
        pnx_bmi088_lab_fault |= fault_chip_id;
    }

    raw_sample previous{};
    bool have_previous = false;
    pnx_bmi088_lab_state =
        pnx_bmi088_lab_fault == 0U ? 2U : 3U;
    for (;;)
    {
        raw_sample sample{};
        if (read_sample(sample))
        {
            if (have_previous &&
                cboard::bmi088_lab::sample_changed(
                    previous, sample, 32))
            {
                ++pnx_bmi088_change_count;
            }
            previous = sample;
            have_previous = true;
            publish(sample);
            ++pnx_bmi088_sample_count;
        }
        else
        {
            pnx_bmi088_lab_fault |= fault_spi_timeout;
            pnx_bmi088_lab_state = 3U;
        }

        ++demo_debug_instance.heartbeat_count;
        demo_debug_instance.threadx_tick =
            static_cast<std::uint32_t>(tx_time_get());
        cboard::demo::sync_threadx(&imu_thread);
        if ((pnx_bmi088_sample_count % 50U) == 0U)
        {
            (void)bsp::indicator::toggle(
                bsp::indicator::channel::green);
        }
        tx_thread_sleep(sample_ticks);
    }
}

} // namespace

extern "C" void app_start(void)
{
    demo_debug_instance.demo_kind =
        static_cast<std::uint32_t>(
            cboard::demo::kind::imu_bmi088_lab);
    const UINT status =
        tx_thread_create(
            &imu_thread, imu_name, imu_entry, 0U,
            imu_stack, sizeof(imu_stack), 10U, 10U,
            TX_NO_TIME_SLICE, TX_AUTO_START);
    demo_debug_instance.start_status = status;
    if (status != TX_SUCCESS)
    {
        pnx_bmi088_lab_state = 3U;
        pnx_bmi088_lab_fault = fault_spi_timeout;
    }
}
