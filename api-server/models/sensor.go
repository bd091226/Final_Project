package models

import "gorm.io/gorm"

type SensorData struct {
	gorm.Model
	DeviceID    string  `json:"device_id"`
	Temperature float32 `json:"temperature"`
	Humidity    float32 `json:"humidity"`
}
