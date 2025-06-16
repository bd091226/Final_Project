#ifndef BCAR_MOTER_H
#define BCAR_MOTER_H

typedef struct {
    int r; // row
    int c; // col
} Point;

typedef enum { N = 0, E = 1, S = 2, W = 3 } Direction; 

void forward_one(Point *pos, int dir, int speed);
void rotate_one(Direction *dir, int turn_dir, int speed);
void motor_go(int speed, double duration);
void motor_stop(void);
void motor_control(int left_dir, int right_dir, int left_speed, int right_speed, int standby, int reset, float seconds);

#endif
