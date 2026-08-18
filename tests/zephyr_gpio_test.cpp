#include <array>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "rtos/Gpio.hpp"
#include "rtos/Queue.hpp"
#include "zephyr_gpio_test.hpp"

namespace
{
class ZephyrGpioTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        zephyr_gpio_test::reset();
    }
};
}

TEST_F(ZephyrGpioTest, MapsUruWearLogicalPinsFromBoardDevicetree)
{
    struct ExpectedPin
    {
        int logicalId;
        int port;
        uint32_t pin;
    };
    constexpr std::array<ExpectedPin, 8> expected{{
        {7, 0, 7},
        {4, 0, 4},
        {26, 0, 26},
        {27, 0, 27},
        {28, 0, 28},
        {24, 0, 24},
        {47, 1, 15},
        {25, 0, 25},
    }};

    rtos::gpio::Config config;
    config.mode = rtos::gpio::Mode::Input;
    for (const auto& value : expected)
    {
        auto pin = rtos::gpio::Pin::make(value.logicalId, config);
        ASSERT_EQ(pin.id(), value.logicalId);
        EXPECT_EQ(zephyr_gpio_test::lastConfigured().port->id, value.port);
        EXPECT_EQ(zephyr_gpio_test::lastConfigured().pin, value.pin);
    }

    EXPECT_EQ(rtos::gpio::Pin::make(0, config).id(), -1);
}

TEST_F(ZephyrGpioTest, BothEdgeInterruptReportsTheSampledDirection)
{
    rtos::gpio::Pin touch = rtos::gpio::Pin::make(7);
    std::vector<rtos::gpio::Event> events;
    touch.set_callback([&events](const rtos::gpio::Event& event) {
        events.push_back(event);
    });
    touch.enable_interrupt(rtos::gpio::Trigger::Both);

    zephyr_gpio_test::fire(0, 7, true);
    zephyr_gpio_test::fire(0, 7, false);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].pin_id, 7);
    EXPECT_EQ(events[0].trigger, rtos::gpio::Trigger::Rising);
    EXPECT_TRUE(events[0].level);
    EXPECT_EQ(events[0].isr_count, 1u);
    EXPECT_EQ(events[1].trigger, rtos::gpio::Trigger::Falling);
    EXPECT_FALSE(events[1].level);
    EXPECT_EQ(events[1].isr_count, 2u);
}

TEST_F(ZephyrGpioTest, AttachedQueueReceivesInterruptThroughIsrSend)
{
    rtos::Queue<rtos::gpio::Event> queue(2);
    rtos::gpio::Pin interrupt = rtos::gpio::Pin::make(26);
    interrupt.attach_queue(&queue);
    interrupt.enable_interrupt(rtos::gpio::Trigger::Rising);

    zephyr_gpio_test::fire(0, 26, true);

    rtos::gpio::Event event{};
    ASSERT_TRUE(queue.try_receive(event));
    EXPECT_TRUE(zephyr_gpio_test::lastQueueSendWasIsr());
    EXPECT_EQ(event.pin_id, 26);
    EXPECT_EQ(event.trigger, rtos::gpio::Trigger::Rising);
    EXPECT_TRUE(event.level);
}
