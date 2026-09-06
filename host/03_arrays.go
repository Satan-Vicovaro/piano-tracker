package main

import (
	"crypto/sha256"
	"fmt"
)

// In Go, [3]int is a distinct type from [4]int
func modifyArray(a [3]int) {
	a[0] = 999 // Mutates local copy!
}

func modifyArrayPointer(a *[3]int) {
	a[0] = 999 // Mutates original via pointer
}

func RunArraysDemo() {
	fmt.Println("==================================================")
	fmt.Println("PART 3: FIXED ARRAYS (Value Types, Fixed Size)")
	fmt.Println("==================================================")

	// Zero-valued array
	var octaves [4]int
	fmt.Println("Zero-valued [4]int:             ", octaves)

	// Counted array [...]
	chords := [...]string{"Major", "Minor", "Diminished"}
	fmt.Printf("Counted array type: %T, value: %v\n", chords, chords)

	// Keyed index initialization
	sparse := [5]int{0: 100, 4: 500}
	fmt.Println("Index-initialized [5]int:       ", sparse)

	// Value semantics (Full copy on assignment)
	original := [3]int{1, 2, 3}
	copied := original
	copied[0] = 42
	fmt.Println("Original after assignment copy: ", original, "(unaffected)")
	fmt.Println("Copied array:                    ", copied)

	modifyArray(original)
	fmt.Println("After modifyArray (by-value):   ", original, "(unaffected)")

	modifyArrayPointer(&original)
	fmt.Println("After modifyArrayPointer (*ptr):", original, "(modified!)")

	// Equality comparison
	a1 := [3]int{1, 2, 3}
	a2 := [3]int{1, 2, 3}
	a3 := [3]int{1, 2, 4}
	fmt.Println("Array equality a1 == a2:        ", a1 == a2) // true
	fmt.Println("Array equality a1 == a3:        ", a1 == a3) // false

	// Fixed array in real-world standard library: SHA-256 returns [32]byte
	hash := sha256.Sum256([]byte("piano key"))
	fmt.Printf("SHA-256 fixed array type %T: %x...\n", hash, hash[:8])
	fmt.Println()
}
