#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    pid_t pidA, pidB,pidStorage;
    int status;

    // 보관함 (Storage) 프로세스 실행
    pidStorage = fork();
    if (pidStorage < 0) {
        perror("fork for Storage failed");
        return 1;
    }
    if (pidStorage == 0) {
        execl("./road_detect", "road_detect", (char*)NULL);
        perror("execl road_detect failed");
        _exit(1);
    }

    // A차 프로세스 실행
    pidA = fork();
    if (pidA < 0) {
        perror("fork for A차 failed");
        return 1;
    }
    if (pidA == 0) {
        // 자식 프로세스: count_mattcheck 실행
        execl("./count_mattcheck", "count_mattcheck", (char*)NULL);
        perror("execl count_mattcheck failed");
        _exit(1);
    }

    // B차 프로세스 실행
    pidB = fork();
    if (pidB < 0) {
        perror("fork for B차 failed");
        return 1;
    }
    if (pidB == 0) {
        // 자식 프로세스: Bcar_C 실행
        execl("./Bcar_C", "Bcar_C", (char*)NULL);
        perror("execl Bcar_C failed");
        _exit(1);
    }

    // 부모 프로세스는 두 자식이 모두 끝나길 기다립니다.
    waitpid(pidStorage, &status, 0);
    printf("보관함 프로세스 종료 (status=%d)\n", status);
    waitpid(pidA, &status, 0);
    printf("A차 프로세스 종료 (status=%d)\n", status);
    waitpid(pidB, &status, 0);
    printf("B차 프로세스 종료 (status=%d)\n", status);

    return 0;
}
