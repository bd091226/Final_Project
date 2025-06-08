/*
 * B_motor_time.c
 * Receives B:TURN_LEFT, B:TURN_RIGHT, B:FORWARD commands on stdin,
 * controls motor via WiringPi, and replies with B:POS row col dir
 *
 * 컴파일:
 *   gcc B_motor_time.c -o B_motor_time -lwiringPi -lm
 * 실행:
 *   ./B_motor_time
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <signal.h>
 #include <wiringPi.h>
 #include <softPwm.h>
 
 // Motor pins for Car B (BCM numbering)
 #define B_L_IN1 5
 #define B_L_IN2 6
 #define B_L_PWM 13
 #define B_R_IN1 19
 #define B_R_IN2 26
 #define B_R_PWM 12
 
 typedef enum { NORTH=0, EAST=1, SOUTH=2, WEST=3 } Direction;
 
 typedef struct { int row, col; Direction dir; } Position;
 
 static Position pos = {0, 0, NORTH};
 
 // Movement timing
 #define SECONDS_PER_GRID_STEP      1.1
 #define SECONDS_PER_90_DEG_ROTATION 0.67
 #define PRE_ROTATE_FORWARD_CM      8.0f
 
 static void motor_go(void) {
     // both wheels forward
     digitalWrite(B_L_IN1, LOW);
     digitalWrite(B_L_IN2, HIGH);
     digitalWrite(B_R_IN1, LOW);
     digitalWrite(B_R_IN2, HIGH);
     softPwmWrite(B_L_PWM, 100);
     softPwmWrite(B_R_PWM, 100);
 }
 static void motor_stop(void) {
     digitalWrite(B_L_IN1, LOW);
     digitalWrite(B_L_IN2, LOW);
     digitalWrite(B_R_IN1, LOW);
     digitalWrite(B_R_IN2, LOW);
     softPwmWrite(B_L_PWM, 0);
     softPwmWrite(B_R_PWM, 0);
 }
 static void motor_left_wheel_rev_right_wheel_fwd(void) {
     // rotate left: left wheel reverse, right wheel forward
     digitalWrite(B_L_IN1, HIGH);
     digitalWrite(B_L_IN2, LOW);
     digitalWrite(B_R_IN1, LOW);
     digitalWrite(B_R_IN2, HIGH);
     softPwmWrite(B_L_PWM, 100);
     softPwmWrite(B_R_PWM, 100);
 }
 static void motor_left_wheel_fwd_right_wheel_rev(void) {
     // rotate right: left forward, right reverse
     digitalWrite(B_L_IN1, LOW);
     digitalWrite(B_L_IN2, HIGH);
     digitalWrite(B_R_IN1, HIGH);
     digitalWrite(B_R_IN2, LOW);
     softPwmWrite(B_L_PWM, 100);
     softPwmWrite(B_R_PWM, 100);
 }
 
 static void handle_sigint(int sig) {
     (void)sig;
     motor_stop();
     exit(0);
 }
 
 static void delay_sec(double sec) {
     usleep((unsigned int)(sec * 1e6));
 }
 
 // Rotate by one 90° step: turn_dir +1 right, -1 left
 static void rotate_one(Direction *d, int turn_dir) {
     // micro forward prior to rotation
     double micro = (PRE_ROTATE_FORWARD_CM / 30.0) * SECONDS_PER_GRID_STEP;
     motor_go(); delay_sec(micro); motor_stop();
     delay_sec(0.1);
 
     if(turn_dir > 0)
         motor_left_wheel_fwd_right_wheel_rev();  // right turn
     else
         motor_left_wheel_rev_right_wheel_fwd();  // left turn
     delay_sec(SECONDS_PER_90_DEG_ROTATION);
     motor_stop();
 
     int nd = (*d + turn_dir + 4) % 4;
     *d = (Direction)nd;
 }
 
 // Move forward one grid cell
 static void forward_one(void) {
     motor_go();
     delay_sec(SECONDS_PER_GRID_STEP);
     motor_stop();
     switch(pos.dir) {
         case NORTH: pos.row -= 1; break;
         case SOUTH: pos.row += 1; break;
         case EAST:  pos.col += 1; break;
         case WEST:  pos.col -= 1; break;
     }
 }
 
 int main(void) {
     signal(SIGINT, handle_sigint);
     wiringPiSetupGpio();
     pinMode(B_L_IN1, OUTPUT);
     pinMode(B_L_IN2, OUTPUT);
     pinMode(B_R_IN1, OUTPUT);
     pinMode(B_R_IN2, OUTPUT);
     softPwmCreate(B_L_PWM, 0, 100);
     softPwmCreate(B_R_PWM, 0, 100);
 
     // send initial position
     printf("B:POS %d %d %d\n", pos.row, pos.col, pos.dir);
     fflush(stdout);
 
     char buf[128];
     while(fgets(buf, sizeof(buf), stdin)) {
         if(strncmp(buf, "B:TURN_LEFT", 11) == 0) {
             rotate_one(&pos.dir, -1);
         } else if(strncmp(buf, "B:TURN_RIGHT", 12) == 0) {
             rotate_one(&pos.dir, +1);
         } else if(strncmp(buf, "B:FORWARD", 9) == 0) {
             forward_one();
         } else {
             continue;
         }
         // send updated position back
         printf("B:POS %d %d %d\n", pos.row, pos.col, pos.dir);
         fflush(stdout);
     }
     return 0;
 }
 