# LAN Poker Game

**Language:** C  

---

## Overview

This project is a multiplayer Texas Hold Em style poker game implemented in C using socket programming. It allows up to six clients to connect and play over a simulated LAN environment via `localhost`. The game manages synchronization and communication between clients to ensure consistent gameplay.

---

## Features

- Multiplayer support for **up to six clients**.  
- **Client-server architecture** with clearly defined communication protocols.  
- Real-time handling of **player actions** (bets, folds, checks, etc.).  
- **Game state synchronization** to ensure all clients see the same table and hand progression.  
- Network messaging to communicate game updates, player turns, and results.

---

## Implementation Details

- Written in **C** using **sockets** for network communication.  
- Server handles **game logic** and broadcasts updates to all connected clients.  
- Clients send user actions to the server and receive game state updates in real time.  
- Designed protocols for:
  - Player actions
  - Game state updates
  - Messaging and notifications

---

## How to Run

1. In terminal: `make tui.client`
   - This is the graphical client, the size of the terminal must be relatively large to properly render.
2. In terminal: `make client.automated`
   - This is the script based client, and automates test cases.
3. To compile the code, run: `make server.poker_server`
4. To start the server, type `./build/server.poker_server #` where # is a random number used to shuffle the deck of cards.
5. To run the graphical client, type `./build/tui.client #` where # is the player you wish to play as between 0 and 5.
6. Enjoy the LAN poker game! Have fun!
