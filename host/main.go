package main

import (
	"bufio"
	"context"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"strings"
	"time"
)

func listenForData(conn net.Conn) error {
	inputBuffer := make([]byte, 1024)

	conn.SetReadDeadline(time.Now().Add(10 * time.Second))
	espAnwser, err := conn.Read(inputBuffer)
	if err != nil {
		return err
	}
	fmt.Printf("Response: %s\n", string(inputBuffer[:espAnwser]))
	return nil
}

func sendMessage(message string, conn net.Conn) error {
	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	_, err := conn.Write([]byte(message))
	if err != nil {
		return err
	}
	return nil
}

func closeOnCancel(ctx context.Context, conn net.Conn) {
	<-ctx.Done()
	conn.Close()
}

func sendLoop(ctx context.Context, conn net.Conn, userMessages <-chan string) {
	for {
		select {
		case <-ctx.Done():
			return
		case message := <-userMessages:
			if err := sendMessage(message, conn); err != nil {
				switch {
				case ctx.Err() != nil || errors.Is(err, net.ErrClosed):
					fmt.Println("Sender stopped due to cancellation.")
					return
				case os.IsTimeout(err):
					fmt.Println("Timeout error sending to esp")
				default:
					fmt.Printf("Unexpected connection error: %v\n", err)
					return
				}
			}
		}
	}
}

func listenLoop(ctx context.Context, conn net.Conn) {
	for {
		if err := listenForData(conn); err != nil {
			switch {
			case ctx.Err() != nil || errors.Is(err, net.ErrClosed):
				fmt.Println("Listener stopped due to cancellation.")
				return
			case errors.Is(err, io.EOF):
				fmt.Println("ESP32 closed the connection.")
				return
			case os.IsTimeout(err):
				// fmt.Println("Read timed out, waiting for next packet...")
			default:
				fmt.Printf("Unexpected connection error: %v\n", err)
				return
			}
		}
	}
}

func espListener(ctx context.Context, userMessages <-chan string) {
	conn, err := net.DialTimeout("tcp", "esp32s3.local:2137", 10*time.Second)
	if err != nil {
		fmt.Printf("Connection failed: %v\n", err)
		return
	}
	defer conn.Close()
	fmt.Printf("Connected! Resolved to: %s\n", conn.RemoteAddr().String())

	go closeOnCancel(ctx, conn)
	go sendLoop(ctx, conn, userMessages)
	listenLoop(ctx, conn)
}

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	userMessagesCh := make(chan string)
	go espListener(ctx, userMessagesCh)

	scanner := bufio.NewScanner(os.Stdin)

loop:
	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.Split(line, " ")
		switch parts[0] {
		case "exit":
			break loop
		case "send":
			userMessagesCh <- strings.Join(parts[1:], " ")
		}
	}
	if err := scanner.Err(); err != nil {
		fmt.Printf("Error reading input: %v\n", err)
	}
}
