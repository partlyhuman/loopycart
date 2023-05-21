Starting point: [2247c16](https://github.com/partlyhuman/loopycart/tree/2247c166d494dd0e4bb428ad2e6dba4c3e7c9117)

**These changes are experimental.**  
Most of them are not intended at all to be in the main design.  
Feel free to implement them better if verified to be useful.

Changes made by kasami (checked = verified):
- [ ] Connect 32Mbit(4MB) flash A20 to cart pin 31 through 1K (SCHEM SYMBOLS EDITED)
- [ ] DIP switches to override high address pin to 0 or 1 for flashing 2M at a time, could be done better
- [ ] Change feature detect pullups to 10K, DIP switches to pull feature detect pins down with 1K, could be done better
- [x] Align cart edge connector properly, it was off center
- [ ] Add a solder jumper on back to select pico GP23/GP3 on A19, allow genuine pico with code changes
- [ ] Clean up board outline a bit
- [x] Replace RAM diode with BAT54C (Schottky lower drop) & correct symbol
