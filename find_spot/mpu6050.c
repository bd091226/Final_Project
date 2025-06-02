#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "mpu6050.h"

#define MPU6050_ADDR 0x68
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B
#define GYRO_XOUT_H 0x43

static int i2c_fd = -1;

int mpu6050_init(const char *i2c_device)
{
    i2c_fd = open(i2c_device, O_RDWR);
    if (i2c_fd < 0)
    {
        perror("I2C open");
        return -1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, MPU6050_ADDR) < 0)
    {
        perror("I2C ioctl");
        return -1;
    }

    char buf[2] = {PWR_MGMT_1, 0x00};
    if (write(i2c_fd, buf, 2) != 2)
    {
        perror("I2C write");
        return -1;
    }

    printf("[MPU6050] 초기화 완료\n");
    return 0;
}

static int read_word(int reg, short *val)
{
    char reg_buf[1] = {reg};
    char data_buf[2] = {0};
    if (write(i2c_fd, reg_buf, 1) != 1)
        return -1;
    if (read(i2c_fd, data_buf, 2) != 2)
        return -1;
    *val = (data_buf[0] << 8) | data_buf[1];
    return 0;
}

int get_acceleration(float *ax, float *ay, float *az)
{
    short x, y, z;
    if (read_word(ACCEL_XOUT_H, &x) < 0)
        return -1;
    if (read_word(ACCEL_XOUT_H + 2, &y) < 0)
        return -1;
    if (read_word(ACCEL_XOUT_H + 4, &z) < 0)
        return -1;
    *ax = x / 16384.0f;
    *ay = y / 16384.0f;
    *az = z / 16384.0f;
    return 0;
}

int get_gyroscope(float *gx, float *gy, float *gz)
{
    short x, y, z;
    if (read_word(GYRO_XOUT_H, &x) < 0)
        return -1;
    if (read_word(GYRO_XOUT_H + 2, &y) < 0)
        return -1;
    if (read_word(GYRO_XOUT_H + 4, &z) < 0)
        return -1;
    *gx = x / 131.0f;
    *gy = y / 131.0f;
    *gz = z / 131.0f;
    return 0;
}
