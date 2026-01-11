
Markdown


# 🚗 Smart Traffic Management System (TMS)

> **An advanced, high-performance autonomous traffic monitoring and control system built on Linux using C Sockets and I/O Multiplexing.**

[![C Language](https://img.shields.io/badge/Language-C-blue.svg)](https://en.cppreference.com/w/c)
[![Socket](https://img.shields.io/badge/Network-TCP%2FIP-orange.svg)](https://datatracker.ietf.org/doc/html/rfc793)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)](https://www.kernel.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

---

## 📋 Table of Contents
1. [Introduction](#-introduction)
2. [System Architecture](#-system-architecture)
3. [Deep Dive: Technical Implementation](#-deep-dive-technical-implementation)
4. [The Network Protocol](#-the-network-protocol)
5. [Safety & Autonomous Logic](#-safety--autonomous-logic)
6. [Installation & Usage](#-installation--usage)
7. [Technical Challenges & Solutions](#-technical-challenges--solutions)

---

## 📖 Introduction
The Smart TMS is designed to simulate an ecosystem of autonomous vehicles communicating with a central traffic authority. Unlike traditional multi-threaded servers, this project implements **Asynchronous I/O Multiplexing** to manage dozens of concurrent connections with a minimal CPU footprint.

---

## 🏗️ System Architecture

The system utilizes a **Stateful Client-Server model**. 



### The Server (The Brain)
* **Single-Process/Single-Threaded:** Uses `select()` to poll all active vehicle sockets.
* **Global Registry:** Maintains a database of all connected vehicles, their current coordinates, and speeds.
* **Alert Broadcaster:** When a collision is reported, the server identifies all vehicles within a specific radius and pushes emergency speed limits.

### The Client (The Vehicle)
* **Autonomous Simulation:** Calculates real-time GPS coordinates based on its internal velocity vector.
* **Telemetry Engine:** Periodically transmits its state to the server without blocking the user input interface.

---

## ⚙️ Deep Dive: Technical Implementation

### 🔄 I/O Multiplexing with `select()`
The core of the application is the non-blocking event loop. Instead of spawning a thread for every car (which would be inefficient for hundreds of cars), we use:

```c
FD_ZERO(&read_fds);
FD_SET(socket_fd, &read_fds);
FD_SET(STDIN_FILENO, &read_fds);

int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);


Why this matters:
Zero Latency: The car continues to move even if no data is received.
Concurrency: The server can scale to handle many connections without the memory overhead of thread stacks.
📐 The Movement Engine
The client simulates physics using a discrete-time approach. Every time select() times out (1 second), the following logic is applied:
$$ \Delta d = v \times \Delta t $$
$$ Lat_{new} = Lat_{old} + (\Delta d \times \cos(\theta)) $$
$$ Long_{new} = Long_{old} + (\Delta d \times \sin(\theta)) $$
Where $\theta$ represents the current heading axis.
📡 The Network Protocol
We use a lightweight, pipe-delimited protocol designed for fast parsing in C.
Direction
Format
Description
C → S
`ID
LAT
S → C
`CMD
NEW_SPEED
C → S
`REPORT
ACCIDENT

Data Integrity & Endianness
To prevent data corruption between different CPU architectures, all multi-byte values (like Ports) are converted using:
htons() / ntohs() (Host to Network Short)
htonl() / ntohl() (Host to Network Long)
🛡️ Safety & Autonomous Logic
🚫 Speed Governor (Auto-Pilot)
The vehicle logic includes a safety callback. When a WARNING packet is parsed:
It enters a "Manual Override" state.
It sets target_speed = min(current_speed, server_limit).
It logs the event to the local terminal for the user.
🗺️ Boundary & Collision Recovery
If a vehicle hits the edge of the simulation map or receives a status of UNKNOWN from the server, it executes a Bumper Protocol:
Reverts to last_known_good_coordinates.
Flips the direction vector ($180^\circ$ turn).
💻 Installation & Usage
Prerequisites
GCC Compiler
Linux Environment (Standard C Libraries)
Building the Project

Bash


make all # Or use the following:
gcc server.c -o server -Wall -O2
gcc client.c -o client -Wall -O2


Running the Simulation
Start the Server:
Bash
./server 8080


Connect a Vehicle:
Bash
./client 127.0.0.1 8080


Initialize Telemetry:
Inside the client terminal, type: set 44.42 26.10 50
🔍 Technical Challenges & Solutions
Challenge
Solution
Ghost Connections
Implemented a Keep-Alive heartbeat; if no telemetry is sent for 10s, the server closes the FD.
String Buffering
Used memset and strncat to prevent buffer overflows and "garbage" characters in JSON parsing.
Graceful Shutdown
Handled SIGINT (Ctrl+C) to properly close sockets and prevent "Address already in use" (TIME_WAIT) errors.

🛠️ Project Structure

Plaintext


├── server.c         # Central Logic & Multiplexing
├── client.c         # Vehicle Simulation & Autonomous Logic
├── common.h         # Shared Constants & Protocol Definitions
├── server_log.json  # Persistent audit trail of all movements
└── Makefile         # Build automation
