This C program demonstrates basic display control using an ATmega328P microcontroller. 
The display is set up in 16-bit color mode, with the entire visible area colored yellow and a green rectangle of size 15x20 pixels is displayed at position 35, 90 (portrait orientation).
The program is extended to handle inputs from Button 1 and Button 2, with debouncing implemented. A timer interrupt, configured via Timer/Counter0 to trigger every millisecond, manages this debouncing.
Button 1 moves the green rectangle one pixel to the left. The rectangle is cleared from its current position and redrawn at the new position. Button 2 moves the green rectangle one pixel to the right.
Holding Button 1 or Button 2 results in continuous and smooth movement of the rectangle in the respective direction. The rectangle is constrained within the display boundaries to prevent it from moving beyond the left or right edges.
