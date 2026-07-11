/**
 * Host FFF example — fake a HAL sensor API with polyontest_fff.h.
 */
#include "polyontest/polyontest.h"
#include "polyontest_fff.h"
#include "sensor.h"

#include <stdint.h>

/* Replace the HAL symbols in this translation unit (no real sensor.c linked). */
POLYONTEST_FAKE_VALUE_FUNC1(int32_t, sensor_read, int, -1)
POLYONTEST_FAKE_VOID_FUNC2(sensor_calibrate, int, int32_t)

/** Production-shaped helper under test: average two channels. */
static int32_t avg_two_channels(int ch_a, int ch_b) {
    int32_t a = sensor_read(ch_a);
    int32_t b = sensor_read(ch_b);
    return (a + b) / 2;
}

static int32_t custom_read_times_ten(int ch) { return (int32_t)(ch * 10); }

TEST(Sensor, Fake, ReturnsConfiguredValue) {
    POLYONTEST_FAKE_RESET_VALUE1(sensor_read, -1);
    sensor_read_return = 3300;
    ASSERT_EQ(3300, sensor_read(0));
    ASSERT_EQ(1, sensor_read_call_count);
    ASSERT_EQ(0, sensor_read_arg0_val);
}

TEST(Sensor, Fake, TracksArgHistory) {
    POLYONTEST_FAKE_RESET_VALUE1(sensor_read, 0);
    sensor_read_return = 100;
    (void)sensor_read(3);
    (void)sensor_read(7);
    ASSERT_EQ(2, sensor_read_call_count);
    ASSERT_EQ(3, sensor_read_arg0_history[0]);
    ASSERT_EQ(7, sensor_read_arg0_history[1]);
    ASSERT_EQ(7, sensor_read_arg0_val);
}

TEST(Sensor, Fake, CustomFakeBody) {
    POLYONTEST_FAKE_RESET_VALUE1(sensor_read, -1);
    sensor_read_custom_fake = custom_read_times_ten;
    ASSERT_EQ(50, sensor_read(5));
    ASSERT_EQ(1, sensor_read_call_count);
}

TEST(Sensor, Fake, VoidFuncAndAvgHelper) {
    POLYONTEST_FAKE_RESET_VOID2(sensor_calibrate);
    POLYONTEST_FAKE_RESET_VALUE1(sensor_read, 0);
    sensor_read_return = 200;
    sensor_calibrate(1, 25);
    ASSERT_EQ(1, sensor_calibrate_call_count);
    ASSERT_EQ(1, sensor_calibrate_arg0_val);
    ASSERT_EQ(25, sensor_calibrate_arg1_val);
    ASSERT_EQ(200, avg_two_channels(0, 1));
    ASSERT_EQ(2, sensor_read_call_count);
}

int main(void) { return polyontest_run_all(); }
