package main

import (
    "github.com/gin-gonic/gin"
    "net/http"
)

func main() {
    // Gin 라우터 생성
    r := gin.Default()

    // GET 요청 핸들러
    r.GET("/ping", func(c *gin.Context) {
        c.JSON(http.StatusOK, gin.H{
            "message": "pong",
        })
    })

    // POST 요청 예시
    r.POST("/submit", func(c *gin.Context) {
        var json struct {
            Name string `json:"name" binding:"required"`
        }

        // JSON 바인딩 및 유효성 검사
        if err := c.ShouldBindJSON(&json); err != nil {
            c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
            return
        }

        c.JSON(http.StatusOK, gin.H{
            "received": json.Name,
        })
    })

    // 경로 파라미터 예시
    r.GET("/user/:id", func(c *gin.Context) {
        id := c.Param("id")
        c.JSON(http.StatusOK, gin.H{
            "id": id,
        })
    })

    // 쿼리 파라미터 예시
    r.GET("/search", func(c *gin.Context) {
        query := c.DefaultQuery("q", "default")
        c.JSON(http.StatusOK, gin.H{
            "query": query,
        })
    })

    // 서버 실행
    r.Run(":8080") // 기본 포트는 8080
}
