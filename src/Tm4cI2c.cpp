/**
 * @file Tm4cI2c.cpp
 * @author Adrian Werel
 * @brief Tm4c I2C Master implementation (Interrupt Driven).
 * @copyright Copyright (c) 2025 noEmbedded
 */

#include <ne/IIrqHandler.h>
#include <ne/ISysclk.h>
#include <ne/Locator.h>
#include <ne/Tm4cI2c.h>
#include <ne/Tm4cSystem.h>
#include <ne/error.h>
#include <tm4c1230h6pm.h>
#include <cstddef>

namespace ne {

Tm4cI2c::Tm4cI2c(std::size_t N, uint32_t speed, std::size_t sclPin, std::size_t sdaPin)
    : rMsa_{getRMsa(N)},
      rMcs_{*(&rMsa_ + (&I2C0_MCS_R - &I2C0_MSA_R))},
      rMdr_{*(&rMsa_ + (&I2C0_MDR_R - &I2C0_MSA_R))},
      rMicr_{*(&rMsa_ + (&I2C0_MICR_R - &I2C0_MSA_R))},
      irq_{getIrq(N)},
      pActive{nullptr},
      pHead{nullptr},
      pTail{nullptr} {
    SYSCTL_RCGCI2C_R |= (1u << N);
    while (0u == (SYSCTL_PRI2C_R & (1u << N))) {
    }

    const PinData scl = getSclPinData(N, sclPin);
    const PinData sda = getSdaPinData(N, sdaPin);

    if (!scl.valid || !sda.valid) {
        error(__FILE__, "Invalid I2C pin configuration.");
    }

    SYSCTL_RCGCGPIO_R |= (1u << scl.port) | (1u << sda.port);
    while (0u == (SYSCTL_PRGPIO_R & (1u << scl.port))) {
    }
    while (0u == (SYSCTL_PRGPIO_R & (1u << sda.port))) {
    }

    auto configPin = [&](const PinData& pd, bool isOpenDrain) {
        volatile uint32_t& rData = getRData(pd.port);
        volatile uint32_t& rAfsel = *(&rData + (&GPIO_PORTA_AFSEL_R - &GPIO_PORTA_DATA_R));
        volatile uint32_t& rDen = *(&rData + (&GPIO_PORTA_DEN_R - &GPIO_PORTA_DATA_R));
        volatile uint32_t& rPctl = *(&rData + (&GPIO_PORTA_PCTL_R - &GPIO_PORTA_DATA_R));
        volatile uint32_t& rOdr = *(&rData + (&GPIO_PORTA_ODR_R - &GPIO_PORTA_DATA_R));

        // Enable alternate function
        rAfsel |= (1u << pd.pin);

        // Set PCTL
        uint32_t mask = (0xFu << (4u * pd.pin));
        uint32_t rctlVal = rPctl;

        rctlVal = (rctlVal & ~mask) | ((pd.pctl & 0xFu) << (4u * pd.pin));
        rPctl = rctlVal;

        // Digital enable
        rDen |= (1u << pd.pin);

        // Open-drain for I2C
        if (isOpenDrain) {
            rOdr |= (1u << pd.pin);
        } else {
            rOdr &= ~(1u << pd.pin);
        }
    };

    configPin(scl, false);

    configPin(sda, true);

    volatile uint32_t& rMtpr_ = {*(&rMsa_ + (&I2C0_MTPR_R - &I2C0_MSA_R))};
    volatile uint32_t& rMimr_ = {*(&rMsa_ + (&I2C0_MIMR_R - &I2C0_MSA_R))};
    volatile uint32_t& rMcr_ = {*(&rMsa_ + (&I2C0_MCR_R - &I2C0_MSA_R))};

    rMcr_ = I2C_MCR_MFE;

    uint32_t sysClk = Locator<ISysclk>::get().getSysclkFrequency();
    uint32_t tpr = (sysClk / (20u * speed));
    if (tpr > 0)
        tpr -= 1u;
    rMtpr_ = tpr;

    ne::Tm4cSystem::getInstance().setIrqHandler(irq_, this);
    rMimr_ |= I2C_MIMR_IM;   // Enable Master Interrupt
}

Tm4cI2c::~Tm4cI2c() {
    ne::Tm4cSystem::getInstance().setIrqHandler(irq_, nullptr);
    volatile uint32_t& rMcr_ = {*(&rMsa_ + (&I2C0_MCR_R - &I2C0_MSA_R))};
    /* Disable master function*/
    rMcr_ = 0u;
}

bool Tm4cI2c::isBusy() const noexcept { return (rMcs_ & I2C_MCS_BUSY) && (state_ != State::IDLE); }

void Tm4cI2c::execute(Transaction& transaction) noexcept {
    transaction.pending = true;
    transaction.pNext = nullptr;
    if (pActive == nullptr) {
        pActive = &transaction;
        startActiveTransaction(transaction);
    } else {
        // Queue the transaction
        if (pTail != nullptr) {
            pTail->pNext = &transaction;
            pTail = &transaction;
        } else {
            pHead = &transaction;
            pTail = &transaction;
        }
    }
}

void Tm4cI2c::startActiveTransaction(Transaction& transaction) noexcept {
    state_ = State::WRITE_REGISTER;
    uint8_t writeAddress = transaction.slaveAddress.data() & 0xFEu;
    if (transaction.size == 0) {
        state_ = State::TX_DATA;
        writeAsync(writeAddress, &transaction.registerAddress, 1, false, false);
        return;
    }
    state_ = State::WRITE_REGISTER;
    writeAsync(writeAddress, &transaction.registerAddress, 1, true, false);
}

void Tm4cI2c::setNextTransaction() noexcept {
    if (pHead != nullptr) {
        pActive = pHead;
        pHead = pHead->pNext;
        if (pHead == nullptr) {
            pTail = nullptr;
        }
        startActiveTransaction(*pActive);
    } else {
        pActive = nullptr;
    }
}

bool Tm4cI2c::writeAsync(uint8_t addr, const uint8_t* data, std::size_t len, bool noStop, bool afterRegister) {
    if (pActive == nullptr)
        return false;
    if (len == 0)
        return false;

    txBuffer_ = data;
    xferLen_ = len;
    xferIdx_ = 0;

    rMsa_ = addr;
    rMdr_ = txBuffer_[xferIdx_++];
    // Start transmission
    if (afterRegister) {
        if (xferLen_ == 1 && !noStop) {
            rMcs_ = I2C_MCS_RUN | I2C_MCS_STOP;
        } else {
            rMcs_ = I2C_MCS_RUN;
        }
    } else {
        if (xferLen_ == 1 && !noStop) {
            rMcs_ = I2C_MCS_START | I2C_MCS_RUN | I2C_MCS_STOP;
        } else {
            rMcs_ = I2C_MCS_START | I2C_MCS_RUN;
        }
    }
    return true;
}

bool Tm4cI2c::readAsync(uint8_t addr, uint8_t* data, std::size_t len) {
    if (pActive == nullptr)
        return false;
    if (len == 0)
        return false;

    rxBuffer_ = data;
    xferLen_ = len;
    xferIdx_ = 0;

    rMsa_ = addr;

    if (xferLen_ == 1) {
        // Single byte read: START, RUN, STOP, NACK
        rMcs_ = I2C_MCS_START | I2C_MCS_RUN | I2C_MCS_STOP;
    } else {
        // Multi byte: START, RUN, ACK
        rMcs_ = I2C_MCS_ACK | I2C_MCS_START | I2C_MCS_RUN;
    }
    return true;
}

void Tm4cI2c::call() noexcept {
    rMicr_ = I2C_MICR_IC;

    if (pActive == nullptr) {
        state_ = State::IDLE;
        return;
    }

    if (rMcs_ & (I2C_MCS_ADRACK | I2C_MCS_DATACK)) {
        pActive->pending = false;
        state_ = State::IDLE;
        rMcs_ = I2C_MCS_STOP;
        setNextTransaction();
        return;
    }

    switch (state_) {
        case State::WRITE_REGISTER: {
            if (pActive->slaveAddress.read()) {
                state_ = State::RX_DATA;
                readAsync(pActive->slaveAddress.data(), pActive->pBuffer, pActive->size);
            } else {
                state_ = State::TX_DATA;
                uint8_t writeAddress = pActive->slaveAddress.data() & 0xFEu;
                writeAsync(writeAddress, pActive->pBuffer, pActive->size, false, true);
            }
            break;
        }
        case State::TX_DATA: {
            if (xferIdx_ < xferLen_) {
                rMdr_ = txBuffer_[xferIdx_++];
                if (xferIdx_ == xferLen_) {
                    rMcs_ = I2C_MCS_RUN | I2C_MCS_STOP;
                } else {
                    rMcs_ = I2C_MCS_RUN;
                }
            } else {
                state_ = State::IDLE;
                pActive->pending = false;
                setNextTransaction();
            }
            break;
        }

        case State::RX_DATA: {
            if (xferIdx_ < xferLen_) {
                rxBuffer_[xferIdx_] = static_cast<uint8_t>(rMdr_ & 0xFFu);
                xferIdx_++;
            }

            if (xferIdx_ < xferLen_) {
                if (xferIdx_ == (xferLen_ - 1)) {
                    rMcs_ = I2C_MCS_STOP | I2C_MCS_RUN;
                } else {
                    rMcs_ = I2C_MCS_ACK | I2C_MCS_RUN;
                }
            } else {
                state_ = State::IDLE;
                pActive->pending = false;
                setNextTransaction();
            }
            break;
        }
        default:
            state_ = State::IDLE;
            pActive->pending = false;
            setNextTransaction();
            break;
    }
}

volatile uint32_t& Tm4cI2c::getRMsa(std::size_t N) const noexcept {
    // clang-format off
    if      (0u == N) { return I2C0_MSA_R; }
    else if (1u == N) { return I2C1_MSA_R; } 
    else if (2u == N) { return I2C2_MSA_R; } 
    else if (3u == N) { return I2C3_MSA_R; } 
    else if (4u == N) { return I2C4_MSA_R; } 
    else if (5u == N) { return I2C5_MSA_R; } 
    else {
        error(__FILE__, "Non-existent I2C.");
        return I2C0_MSA_R;
    }
    // clang-format on
}

std::size_t Tm4cI2c::getIrq(std::size_t N) const noexcept {
    // clang-format off
    if (0u == N) { return INT_I2C0 - INT_GPIOA; } 
    else if (1u == N) { return INT_I2C1 - INT_GPIOA; } 
    else if (2u == N) { return INT_I2C2 - INT_GPIOA; } 
    else if (3u == N) { return INT_I2C3 - INT_GPIOA; } 
    else if (4u == N) { return INT_I2C4 - INT_GPIOA; } 
    else if (5u == N) { return INT_I2C5 - INT_GPIOA; } 
    else {
        error(__FILE__, "Non-existent I2C.");
        return 0;
    }
    // clang-format on
}

Tm4cI2c::PinData Tm4cI2c::getSclPinData(std::size_t N, std::size_t pin) const noexcept {
    // Port: 0=A, 1=B, 2=C, 3=D, 4=E, 5=F, 6=G
    // clang-format off
    if      ((0u == N) && (47u == pin)) { return {true, 1u, 2u, 3u}; }  // I2C0 SCL: PB2 } 
    else if ((1u == N) && (23u == pin)) { return {true, 0u, 6u, 3u}; }  // I2C1 SCL: PA6 }
    else if ((1u == N) && (33u == pin)) { return {true, 6u, 4u, 3u}; }  // I2C1 SCL: PG4 }
    else if ((2u == N) && (59u == pin)) { return {true, 4u, 4u, 3u}; }  // I2C2 SCL: PE4 }
    else if ((3u == N) && (61u == pin)) { return {true, 3u, 0u, 3u}; }  // I2C3 SCL: PD0 }
    else if ((3u == N) && (37u == pin)) { return {true, 6u, 0u, 3u}; }  // I2C3 SCL: PG0 }
    else if ((4u == N) && (35u == pin)) { return {true, 6u, 2u, 3u}; }  // I2C4 SCL: PG2 }
    else if ((5u == N) && (1u == pin)) { return {true, 1u, 6u, 3u}; }  // I2C5 SCL: PB6 }
    return {false, 0u, 0u, 0u};
    // clang-format on
}

Tm4cI2c::PinData Tm4cI2c::getSdaPinData(std::size_t N, std::size_t pin) const noexcept {
    // clang-format off
    if      ((0u == N) && (48u == pin)) { return {true, 1u, 3u, 3u}; }  // I2C0 SDA: PB3
    else if ((1u == N) && (24u == pin)) { return {true, 0u, 7u, 3u}; }  // I2C1 SDA: PA7
    else if ((1u == N) && (32u == pin)) { return {true, 6u, 5u, 3u}; }  // I2C1 SDA: PG5
    else if ((2u == N) && (60u == pin)) { return {true, 4u, 5u, 3u}; }  // I2C2 SDA: PE5
    else if ((3u == N) && (62u == pin)) { return {true, 3u, 1u, 3u}; }  // I2C3 SDA: PD1
    else if ((3u == N) && (36u == pin)) { return {true, 6u, 1u, 3u}; }  // I2C3 SDA: PG1
    else if ((4u == N) && (34u == pin)) { return {true, 6u, 3u, 3u}; }  // I2C4 SDA: PG3
    else if ((5u == N) && (4u == pin)) { return {true, 1u, 7u, 3u}; }  // I2C5 SDA: PB7
    return {false, 0u, 0u, 0u};
    // clang-format on
}

volatile uint32_t& Tm4cI2c::getRData(std::size_t port) const noexcept {
    // clang-format off
    if      (0u == port) { return GPIO_PORTA_DATA_R; } 
    else if (1u == port) { return GPIO_PORTB_DATA_R; } 
    else if (2u == port) { return GPIO_PORTC_DATA_R; } 
    else if (3u == port) { return GPIO_PORTD_DATA_R; } 
    else if (4u == port) { return GPIO_PORTE_DATA_R; } 
    else if (5u == port) { return GPIO_PORTF_DATA_R; } 
    else if (6u == port) { return GPIO_PORTG_DATA_R; }
    else {
        error(__FILE__, "Non-existent port.");
        return GPIO_PORTA_DATA_R;
    }
    // clang-format on
}

}   // namespace ne
