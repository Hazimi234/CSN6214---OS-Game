# CSN6214---OS-Game

# Compile the entire project
gcc server.c game_logic.c -o server -lpthread -lrt && gcc client.c game_logic.c -o client -lpthread -lrt

# Run the server
./server

# Run multiple clients (one per player)
./client  # Player 1 (Host)
./client  # Player 2
./client  # Player 3