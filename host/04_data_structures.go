package main

import (
	"container/list"
	"fmt"
	"unsafe"
)

func RunDataStructuresDemo() {
	fmt.Println("==================================================")
	fmt.Println("PART 4: DYNAMIC ARRAYS & DATA STRUCTURES")
	fmt.Println("==================================================")

	// -------------------------------------------------------------
	// 1. DYNAMIC ARRAYS: SLICES (The std::vector of Go)
	// -------------------------------------------------------------
	fmt.Println("--- 1. Slices: Dynamic Resizing, len vs cap ---")

	// make([]Type, len, cap)
	// len = elements currently initialized
	// cap = total underlying memory allocated before needing a reallocation
	dynArr := make([]int, 0, 4)
	fmt.Printf("Initial make: len=%d, cap=%d, data=%v\n", len(dynArr), cap(dynArr), dynArr)

	// Watch capacity grow dynamically as we append
	for i := 1; i <= 6; i++ {
		dynArr = append(dynArr, i*10)
		fmt.Printf("Appended %2d -> len=%d, cap=%d, address=%p\n",
			i*10, len(dynArr), cap(dynArr), dynArr)
	}

	// copy() - Deep copy a slice so they don't share underlying memory
	independentCopy := make([]int, len(dynArr))
	copy(independentCopy, dynArr)
	independentCopy[0] = 777
	fmt.Printf("After copy: original[0]=%d, copy[0]=%d (independent!)\n", dynArr[0], independentCopy[0])

	// -------------------------------------------------------------
	// 2. STACK (LIFO) & QUEUE (FIFO) VIA SLICES
	// -------------------------------------------------------------
	fmt.Println("\n--- 2. Stack and Queue using Slices ---")

	// Stack: Push with append, Pop from end
	var stack []string
	stack = append(stack, "Note C4") // Push
	stack = append(stack, "Note E4")
	stack = append(stack, "Note G4")

	// Pop
	top := stack[len(stack)-1]
	stack = stack[:len(stack)-1]
	fmt.Printf("Stack Popped: %s | Remaining: %v\n", top, stack)

	// Queue: Enqueue with append, Dequeue from front
	var queue []string
	queue = append(queue, "Event 1") // Enqueue
	queue = append(queue, "Event 2")
	queue = append(queue, "Event 3")

	// Dequeue
	front := queue[0]
	queue = queue[1:]
	fmt.Printf("Queue Dequeued: %s | Remaining: %v\n", front, queue)

	// -------------------------------------------------------------
	// 3. HASH MAPS: map[Key]Value
	// -------------------------------------------------------------
	fmt.Println("\n--- 3. Hash Maps (O(1) lookups) ---")
	keyMap := make(map[string]int)
	keyMap["Middle C"] = 60
	keyMap["Concert A"] = 69

	// Lookup using comma-ok idiom
	val, ok := keyMap["Middle C"]
	fmt.Printf("Lookup 'Middle C': MIDI number = %d, exists = %t\n", val, ok)

	// Deleting a key
	delete(keyMap, "Middle C")
	_, okAfterDelete := keyMap["Middle C"]
	fmt.Printf("After delete: exists = %t\n", okAfterDelete)

	// -------------------------------------------------------------
	// 4. SETS (Zero-Memory map[T]struct{})
	// -------------------------------------------------------------
	fmt.Println("\n--- 4. Sets in Go (map[T]struct{}) ---")
	// Go does not have a native 'set' type.
	// We use map[Key]struct{} because empty struct struct{} uses ZERO bytes!
	fmt.Printf("Size of struct{}{} in memory: %d bytes\n", unsafe.Sizeof(struct{}{}))

	activeChords := make(map[string]struct{})
	// Add to set:
	activeChords["Cmaj"] = struct{}{}
	activeChords["Amin"] = struct{}{}

	// Check set membership:
	query := "Cmaj"
	if _, exists := activeChords[query]; exists {
		fmt.Printf("Set contains '%s': true\n", query)
	}

	// -------------------------------------------------------------
	// 5. DOUBLY LINKED LIST (container/list)
	// -------------------------------------------------------------
	fmt.Println("\n--- 5. Doubly-Linked List (container/list) ---")
	l := list.New()
	l.PushBack("First MIDI Packet")
	second := l.PushBack("Second MIDI Packet")
	l.PushFront("Header Packet") // Insert at front

	// Insert after a specific element
	l.InsertAfter("Interleaved Packet", second)

	// Traverse the list:
	for e := l.Front(); e != nil; e = e.Next() {
		fmt.Printf("  [Node]: %v\n", e.Value)
	}
	fmt.Println()
}
