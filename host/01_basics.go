package main

import (
	"errors"
	"fmt"
	"sync"
	"time"
)

// Struct: Go's custom data model (no classes/inheritance)
type Note struct {
	Name     string
	Octave   int
	Duration float64 // in seconds
}

// Value receiver: operates on a copy (does not mutate original)
func (n Note) String() string {
	return fmt.Sprintf("%s%d (%.2fs)", n.Name, n.Octave, n.Duration)
}

// Pointer receiver: can mutate original struct
func (n *Note) Transpose(semitones int) {
	n.Octave += semitones / 12
}

// Idiomatic error handling: returns (Result, error)
func ParseNote(name string, octave int) (Note, error) {
	if octave < 0 || octave > 8 {
		return Note{}, errors.New("octave must be between 0 and 8")
	}
	return Note{
		Name:     name,
		Octave:   octave,
		Duration: 0.5,
	}, nil
}

func RunBasicsDemo() {
	fmt.Println("==================================================")
	fmt.Println("PART 1: GO BASICS (Structs, Methods, Errors, Goroutines)")
	fmt.Println("==================================================")

	// Short variable declaration (inferred type)
	greeting := "Welcome to Go!"
	fmt.Println(greeting)

	// Structs & Methods
	note, err := ParseNote("C", 4)
	if err != nil {
		fmt.Printf("Error: %v\n", err)
		return
	}
	fmt.Println("Created Note:   ", note)
	note.Transpose(12)
	fmt.Println("After Transpose:", note)

	// Concurrency: Goroutine + Channel
	ch := make(chan string)
	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		time.Sleep(50 * time.Millisecond)
		ch <- "Background goroutine: Piano key C4 sounded!"
	}()

	msg := <-ch
	fmt.Println(msg)
	wg.Wait()
	fmt.Println()
}
