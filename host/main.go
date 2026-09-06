package main

import (
	"bufio"
	"fmt"
	"net"
	"os"
	"time"
)

func main() {
	conn, err := net.DialTimeout("tcp", "esp32s3.local:2137", 10*time.Second)
	if err != nil {
		fmt.Printf("Connection failed: %v\n", err)
		return
	}
	defer conn.Close()
	fmt.Printf("Connected! Resolved to: %s\n", conn.RemoteAddr().String())

	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		line := scanner.Text()
		if line == "q" || line == "exit" {
			break
		}
		fmt.Printf("Sending to esp: %v\n", line)

		conn.Write([]byte(line))

		inputBuffer := make([]byte, 1024)

		conn.SetReadDeadline(time.Now().Add(10 * time.Second))
		espAnwser, err := conn.Read(inputBuffer)
		if err != nil {
			fmt.Println("Could not read anwser from esp")
		}
		fmt.Printf("Response: %s\n", string(inputBuffer[:espAnwser]))
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "Error reading input: %v\n", err)
		os.Exit(1)
	}
}
