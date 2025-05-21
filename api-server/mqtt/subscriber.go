package mqtt

import (
	"encoding/json"
	"log"

	"github.com/baboyiban/kosta-2/api-server/models"
	paho_mqtt_golang "github.com/eclipse/paho.mqtt.golang"
	"gorm.io/gorm"
)

func StartMQTTSubscriber(db *gorm.DB, broker string, topic string) {
	opts := paho_mqtt_golang.NewClientOptions().AddBroker(broker)
	opts.SetClientID("gin-mqtt-server")

	client := paho_mqtt_golang.NewClient(opts)
	if token := client.Connect(); token.Wait() && token.Error() != nil {
		log.Fatalf("MQTT Connect Error: %v", token.Error())
	}

	token := client.Subscribe(topic, 1, func(client paho_mqtt_golang.Client, msg paho_mqtt_golang.Message) {
		log.Printf("Received on topic [%s]: %s", msg.Topic(), string(msg.Payload()))

		var payload struct {
			DeviceID    string  `json:"device_id"`
			Temperature float64 `json:"temperature"`
			Humidity    float64 `json:"humidity"`
		}

		if err := json.Unmarshal(msg.Payload(), &payload); err != nil {
			log.Printf("JSON Unmarshal Error: %v", err)
			return
		}

		sensor := models.SensorData{
			DeviceID:    payload.DeviceID,
			Temperature: float32(payload.Temperature),
			Humidity:    float32(payload.Humidity),
		}

		result := db.Create(&sensor)
		if result.Error != nil {
			log.Printf("DB Insert Error: %v", result.Error)
		} else {
			log.Printf("Saved sensor data: %+v", sensor)
		}
	})

	if token.Wait() && token.Error() != nil {
		log.Printf("Subscribe Error: %v", token.Error())
	}
}
