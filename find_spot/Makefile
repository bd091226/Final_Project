# 컴파일할 C 파일, C++ 래퍼 오브젝트
SRCS       := motor_test.c
WRAPPER_OBJ:= imu_wrapper.o

# 컴파일러·링커 설정
CC         := gcc
CXX        := g++
CFLAGS     := -Wall -O2                     # C 컴파일 옵션
CXXFLAGS   := -Wall -O2                     # C++ 컴파일 옵션
LDFLAGS    := -lstdc++ -lRTIMULib -lwiringPi -lm
# (헤더가 /usr/include/RTIMULib.h에 있을 경우에는 별도 -I 옵션 불필요)
# 만약 /usr/local/include에 있다면:  CXXFLAGS += -I/usr/local/include

# 최종 생성될 실행 파일 이름
TARGET     := motor_test

# -----------------------------
.PHONY: all clean run

# 기본 타겟: 래퍼 컴파일 + 실행 파일 생성
all: $(WRAPPER_OBJ) $(TARGET)

# 1) C++ 래퍼(imu_wrapper.cpp) → 오브젝트 생성
$(WRAPPER_OBJ): imu_wrapper.cpp imu_wrapper.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 2) C 코드 + 래퍼 오브젝트 링크 → 실행 파일 생성
$(TARGET): motor_test.c $(WRAPPER_OBJ)
	$(CC) $(CFLAGS) $< $(WRAPPER_OBJ) -o $@ $(LDFLAGS)

# 3) 실행 편의를 위한 단축 명령 (make run → ./motor_test)
run: all
	@echo "▶▶▶ ./$(TARGET) 실행 중..."
	@sudo ./$(TARGET)

# 4) 빌드 결과물 일괄 삭제
clean:
	@echo "▶▶▶ 빌드 결과물 삭제 중..."
	rm -f $(WRAPPER_OBJ) $(TARGET)

# -----------------------------
