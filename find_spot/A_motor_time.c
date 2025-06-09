/*
 * A_motor_time.c
 * Receives A:TURN_LEFT, A:TURN_RIGHT, A:FORWARD commands on stdin,
 * controls motor via WiringPi, and replies with A:POS row col dir
 *
 * 컴파일:
 *   gcc A_motor_time.c -o A_motor_time -lwiringPi -lm
 * 실행:
 *   ./A_motor_time
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <signal.h>
 #include <wiringPi.h>
 #include <softPwm.h>
 
 #define AIN1 22
 #define AIN2 27
 #define PWMA 18
 #define BIN1 25
 #define BIN2 24
 #define PWMB 23
 
 typedef enum { NORTH=0, EAST=1, SOUTH=2, WEST=3 } Direction;
 
 typedef struct { int row, col; Direction dir; } Position;
 static Position pos = {8, 8, NORTH};
 
 // Movement timing
 #define SECONDS_PER_GRID_STEP      1.1
 #define SECONDS_PER_90_DEG_ROTATION 0.8
 #define PRE_ROTATE_FORWARD_CM      8.0f
 
 static void motor_go(void) {
     digitalWrite(AIN1, LOW);
     digitalWrite(AIN2, HIGH);
     digitalWrite(BIN1, LOW);
     digitalWrite(BIN2, HIGH);
     softPwmWrite(PWMA, 100);
     softPwmWrite(PWMB, 100);
 }
 static void motor_stop(void) {
     digitalWrite(AIN1, LOW);
     digitalWrite(AIN2, LOW);
     digitalWrite(BIN1, LOW);
     digitalWrite(BIN2, LOW);
     softPwmWrite(PWMA, 0);
     softPwmWrite(PWMB, 0);
 }
 static void motor_right(void) {
     digitalWrite(AIN1, LOW);
     digitalWrite(AIN2, HIGH);
     digitalWrite(BIN1, HIGH);
     digitalWrite(BIN2, LOW);
     softPwmWrite(PWMA, 100);
     softPwmWrite(PWMB, 100);
 }
 static void motor_left(void) {
     digitalWrite(AIN1, HIGH);
     digitalWrite(AIN2, LOW);
     digitalWrite(BIN1, LOW);
     digitalWrite(BIN2, HIGH);
     softPwmWrite(PWMA, 100);
     softPwmWrite(PWMB, 100);
 }
 
 static void handle_sigint(int sig) {
     (void)sig;
     motor_stop();
     exit(0);
 }
 
 static void delay_sec(double sec) {
     usleep((unsigned int)(sec * 1e6));
 }
 
 // Rotate one 90° step: +1 right, -1 left
 static void rotate_one(Direction *d, int turn_dir) {
     // micro forward before rotation

    //  double micro = (PRE_ROTATE_FORWARD_CM / 30.0) * SECONDS_PER_GRID_STEP;
    //  motor_go(); delay_sec(micro); motor_stop();
    //  delay_sec(0.1);
    //  if(turn_dir > 0) motor_right(); else motor_left();
    //  delay_sec(SECONDS_PER_90_DEG_ROTATION);
    //  motor_stop();

     int nd = (*d + turn_dir + 4) % 4;
     *d = (Direction)nd;
 }
 
 // Move forward one grid cell
 static void forward_one(void) {
    //  motor_go();
    //  delay_sec(SECONDS_PER_GRID_STEP);
    //  motor_stop();
     switch(pos.dir) {
         case NORTH: pos.row -= 1; break;
         case SOUTH: pos.row += 1; break;
         case EAST:  pos.col += 1; break;
         case WEST:  pos.col -= 1; break;
     }
 }
 
 int main(void) {
     char buf[128];
     signal(SIGINT, handle_sigint);
     wiringPiSetupGpio();
     pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
     pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
     softPwmCreate(PWMA, 0, 100);
     softPwmCreate(PWMB, 0, 100);
 
     // send initial position
     printf("A:POS %d %d %d\n", pos.row, pos.col, pos.dir);
     fflush(stdout);
 
     while(fgets(buf, sizeof(buf), stdin)) {
         // echo received command on terminal
         printf("[A_motor_time] Received command: %s", buf);
         fflush(stdout);
         if(strncmp(buf, "A:TURN_LEFT", 11) == 0) {
             rotate_one(&pos.dir, -1);
         } else if(strncmp(buf, "A:TURN_RIGHT", 12) == 0) {
             rotate_one(&pos.dir, +1);
         } else if(strncmp(buf, "A:FORWARD", 9) == 0) {
             forward_one();
         } else {
             continue;
         }
         // send updated position
         printf("A:POS %d %d %d\n", pos.row, pos.col, pos.dir);
         fflush(stdout);
     }
     return 0;
 }
 