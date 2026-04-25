#pragma once

/**
 * @file Tm4cI2c.h
 * @author Adrian Werel (adrian.werel@noembedded.com)
 * @brief Tm4c I2C Master (Interrupt Driven).
 * @copyright Copyright (c) 2025 noEmbedded
 */

#include <ne/ICallback.h>
#include <ne/II2c.h>
#include <cstddef>
#include <cstdint>

namespace ne {

class Tm4cI2c : public ICallback<void>, public II2c {
public:
    /**
     * @brief Initialize I2C module.
     * Allowed pin numbers for hardware I2Cs (example mapping):
     * +------+----+------------+------------+
     * | I2C  | N  |   sclPin   |   sdaPin   |
     * +------+----+------------+------------+
     * | I2C0 | 0u | 47u        | 48u        |
     * | I2C1 | 1u | 23u or 33u | 24u or 32u |
     * | I2C2 | 2u | 59u        | 60u        |
     * | I2C3 | 3u | 37u or 61u | 36u or 62u |
     * | I2C4 | 4u | 35u        | 34u        |
     * | I2C5 | 5u | 1u         | 4u         |
     * +------+----+------------+------------+
     * ...
     *
     * @param N Hardware I2C identifier.
     * @param speed Desired I2C speed (e.g., 100000u or 400000u).
     * @param sclPin Pin number for SCL.
     * @param sdaPin Pin number for SDA.
     */
    Tm4cI2c(std::size_t N, uint32_t speed, std::size_t sclPin, std::size_t sdaPin);

    /**
     * @brief Disables interrupts and master function
     */
    virtual ~Tm4cI2c();

    /**
     * @brief @see ICallback
     *
     */
    void call() noexcept override;

    /**
     * @brief @see II2c
     *
     */
    void execute(Transaction& transaction) noexcept override;

private:
    struct PinData {
        bool valid;         ///< Is combination valid?
        std::size_t port;   ///< GPIO Port index (0=A, 1=B, etc.)
        std::size_t pin;    ///< Pin number (0-7)
        uint32_t pctl;      ///< PCTL value for this function
    };

    enum class State {
        IDLE,             ///< Bus is free,
        WRITE_REGISTER,   ///< Writing register address (before data)
        TX_DATA,          ///< Transmitting data
        RX_DATA,          ///< Receiving data
    };

    volatile uint32_t& getRMsa(std::size_t N) const noexcept;
    volatile uint32_t& getRData(std::size_t port) const noexcept;

    std::size_t getIrq(std::size_t N) const noexcept;

    PinData getSclPinData(std::size_t N, std::size_t pin) const noexcept;
    PinData getSdaPinData(std::size_t N, std::size_t pin) const noexcept;

    void startActiveTransaction(Transaction& transaction) noexcept;
    void setNextTransaction() noexcept;
    bool writeAsync(uint8_t address, const uint8_t* data, std::size_t len, bool noStop, bool afterRegister);
    bool readAsync(uint8_t address, uint8_t* data, std::size_t len);
    bool isBusy() const noexcept;

    volatile uint32_t& rMsa_;
    volatile uint32_t& rMcs_;
    volatile uint32_t& rMdr_;
    volatile uint32_t& rMicr_;

    const std::size_t irq_;

    Transaction* pActive;
    Transaction* pHead;
    Transaction* pTail;

    volatile State state_{State::IDLE};

    const uint8_t* txBuffer_{nullptr};
    uint8_t* rxBuffer_{nullptr};

    std::size_t xferLen_{0};   // How many bytes to transfer
    std::size_t xferIdx_{0};   // How many bytes already processed
};

}   // namespace ne
