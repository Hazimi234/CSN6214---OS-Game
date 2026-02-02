# CSN6214---OS-Game

# Compile the entire project
gcc -o server server.c game_logic.c shared.h -lpthread -lrt

# Run the server
./server

# Run multiple clients (one per player)
./client  # Player 1 (Host)
./client  # Player 2
./client  # Player 3