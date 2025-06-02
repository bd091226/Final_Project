#ifndef MPU6050_H
#define MPU6050_H

int mpu6050_init(const char *i2c_device);
int get_acceleration(float *ax, float *ay, float *az);
int get_gyroscope(float *gx, float *gy, float *gz);

#endif
