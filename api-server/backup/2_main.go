package main

import (
	"github.com/gin-gonic/gin"
	"gorm.io/driver/mysql"
	"gorm.io/gorm"
)

// User 모델 정의
type User struct {
	gorm.Model
	Name  string `json:"name" binding:"required"`
	Email string `json:"email" binding:"required,email" gorm:"unique"`
}

// 환경 변수 대신 직접 DSN 설정
const dsn = "user:password@tcp(127.0.0.1:3306)/mydb?charset=utf8mb4&parseTime=True&loc=Local"

func main() {
	// MySQL 연결
	db, err := gorm.Open(mysql.Open(dsn), &gorm.Config{})
	if err != nil {
		panic("failed to connect database")
	}

	// 테이블 자동 생성
	db.AutoMigrate(&User{})

	// Gin 라우터 설정
	r := gin.Default()

	// 라우트 그룹
	api := r.Group("/api")
	{
		api.POST("/users", func(c *gin.Context) { createUser(c, db) })
		api.GET("/users", func(c *gin.Context) { getUsers(c, db) })
		api.GET("/users/:id", func(c *gin.Context) { getUser(c, db) })
		api.PUT("/users/:id", func(c *gin.Context) { updateUser(c, db) })
		api.DELETE("/users/:id", func(c *gin.Context) { deleteUser(c, db) })
	}

	// 서버 실행
	r.Run(":8080")
}

// 사용자 생성
func createUser(c *gin.Context, db *gorm.DB) {
	var user User
	if err := c.ShouldBindJSON(&user); err != nil {
		c.AbortWithStatusJSON(400, gin.H{"error": err.Error()})
		return
	}

	if result := db.Create(&user); result.Error != nil {
		c.AbortWithStatusJSON(500, gin.H{"error": result.Error.Error()})
		return
	}

	c.JSON(201, user)
}

// 모든 사용자 조회
func getUsers(c *gin.Context, db *gorm.DB) {
	var users []User
	db.Find(&users)
	c.JSON(200, users)
}

// 특정 사용자 조회
func getUser(c *gin.Context, db *gorm.DB) {
	id := c.Param("id")
	var user User

	if result := db.First(&user, id); result.Error != nil {
		c.AbortWithStatusJSON(404, gin.H{"error": "User not found"})
		return
	}

	c.JSON(200, user)
}

// 사용자 수정
func updateUser(c *gin.Context, db *gorm.DB) {
	id := c.Param("id")
	var user User

	if err := c.ShouldBindJSON(&user); err != nil {
		c.AbortWithStatusJSON(400, gin.H{"error": err.Error()})
		return
	}

	var existingUser User
	if result := db.First(&existingUser, id); result.Error != nil {
		c.AbortWithStatusJSON(404, gin.H{"error": "User not found"})
		return
	}

	existingUser.Name = user.Name
	existingUser.Email = user.Email

	db.Save(&existingUser)
	c.JSON(200, existingUser)
}

// 사용자 삭제
func deleteUser(c *gin.Context, db *gorm.DB) {
	id := c.Param("id")
	var user User

	if result := db.Delete(&user, id); result.RowsAffected == 0 {
		c.AbortWithStatusJSON(404, gin.H{"error": "User not found"})
		return
	}

	c.JSON(200, gin.H{"message": "User deleted successfully"})
}
