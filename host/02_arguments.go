package main

import (
	"fmt"
)

type Config struct {
	Volume int
	Device string
}

// Pass-by-value: receives a full COPY (like C and C++)
func updateConfigValue(c Config) {
	c.Volume = 100 // Only modifies the local copy!
}

// Pass-by-pointer: receives a copy of the address (*Config)
// Dot syntax automatically dereferences struct pointers in Go
func updateConfigPointer(c *Config) {
	c.Volume = 100
}

// Slice header copy: elements are accessible via pointer, but header (len/cap) is local
func mutateSliceElements(nums []int) {
	if len(nums) > 0 {
		nums[0] = 999 // Mutates underlying array
	}
}

func tryAppendSlice(nums []int) {
	nums = append(nums, 888) // Modifies local len/cap only; caller sees nothing
}

// Multiple return values
func getNoteInfo() (string, int, error) {
	return "A", 440, nil
}

// Accepting multiple return values directly from another function
func printNoteInfo(name string, freq int, err error) {
	if err != nil {
		fmt.Println("Error:", err)
		return
	}
	fmt.Printf("Received note directly: %s at %d Hz\n", name, freq)
}

// Passing functions as arguments (Closures)
type FilterFunc func(int) bool

func filterNumbers(items []int, predicate FilterFunc) []int {
	var result []int
	for _, item := range items {
		if predicate(item) {
			result = append(result, item)
		}
	}
	return result
}

func RunArgumentsDemo() {
	fmt.Println("==================================================")
	fmt.Println("PART 2: ARGUMENT PASSING (Go vs C vs C++)")
	fmt.Println("==================================================")

	cfg := Config{Volume: 50, Device: "MIDI Keyboard"}

	updateConfigValue(cfg)
	fmt.Printf("After updateConfigValue (by-value):   Volume = %d (unchanged)\n", cfg.Volume)

	updateConfigPointer(&cfg)
	fmt.Printf("After updateConfigPointer (by-pointer): Volume = %d (modified)\n", cfg.Volume)

	// Slices gotcha
	samples := []int{10, 20, 30}
	fmt.Println("\nOriginal slice:            ", samples)
	mutateSliceElements(samples)
	fmt.Println("After mutateSliceElements: ", samples, "<- element changed!")
	tryAppendSlice(samples)
	fmt.Println("After tryAppendSlice:      ", samples, "<- append did not affect caller!")

	// Function to Function piping
	fmt.Println("\nFunction to Function piping:")
	printNoteInfo(getNoteInfo())

	// Closures / Higher-order functions
	rawVelocities := []int{30, 85, 120, 45, 95}
	loudNotes := filterNumbers(rawVelocities, func(v int) bool {
		return v > 80
	})
	fmt.Println("\nHigher-order filter (> 80):", loudNotes)
	fmt.Println()
}
