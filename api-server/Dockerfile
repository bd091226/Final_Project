# 빌드 단계
FROM golang:1.24-bookworm as builder

WORKDIR /app
COPY . .

# 의존성 다운로드
RUN go mod download

# 애플리케이션 빌드
RUN CGO_ENABLED=0 GOOS=linux go build -o /main .

# 실행 단계 – 최소 Ubuntu 기반
FROM ubuntu:24.04

WORKDIR /app

# 필수 패키지 설치
RUN apt-get update && \
    apt-get install -y ca-certificates tzdata && \
    rm -rf /var/lib/apt/lists/*

# 바이너리 복사
COPY --from=builder /main /app/main

EXPOSE 8080
CMD ["/app/main"]
