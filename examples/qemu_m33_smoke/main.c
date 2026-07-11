/**
 * PolyOnTest QEMU Cortex-M33 smoke — Core stream over UART0.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "board_uart.h"
#include "polyontest/polyontest.h"

#include <stdint.h>

void board_qemu_exit(int code);

static int add(int a, int b) { return a + b; }

static void uart_writer(const void *data, size_t len, void *user) {
    (void)user;
    board_uart_write(data, len);
}

TEST(QemuM33, Core, AddPositive) {
    ASSERT_EQ(5, add(2, 3));
}

TEST(QemuM33, Core, Truth) {
    ASSERT_TRUE(1);
}

TEST(QemuM33, Core, NotNull) {
    int x = 7;
    ASSERT_NOT_NULL(&x);
}

int main(void) {
    board_uart_init();
    polyontest_set_writer(uart_writer, 0);
    int rc = polyontest_run_all();
    board_qemu_exit(rc);
    return rc;
}
