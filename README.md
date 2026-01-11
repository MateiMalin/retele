
---
# 🚗 Smart Traffic Management System (TMS)

> **An advanced autonomous traffic monitoring and control system built using C Sockets and Linux I/O Multiplexing.**

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](#)
[![Socket](https://img.shields.io/badge/Network-TCP%2FIP-orange.svg)](#)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)](#)

---

## 📋 Table of Contents
1. [Introduction](#introduction)
2. [System Architecture](#system-architecture)
3. [Technical Implementation](#technical-implementation)
4. [Communication Protocol](#communication-protocol)
5. [Autonomous Safety Logic](#autonomous-safety-logic)
6. [Installation & Usage](#installation--usage)
7. [Technical Challenges & Solutions](#technical-challenges--solutions)

---

## 📖 Introduction
The Smart TMS is a simulation of an intelligent urban ecosystem where autonomous vehicles communicate with a central traffic authority. The project focuses on high-efficiency networking, utilizing **Asynchronous I/O Multiplexing** to manage concurrent vehicle telemetry without the overhead of multi-threading.

---

## 🏗️ System Architecture

The system utilizes a **Stateful Client-Server** model over the TCP/IP stack.



### 1. The Traffic Authority (Server)
* **Single-Process Event Loop:** Uses the `select()` system call to monitor all active vehicle connections.
* **Registry Management:** Tracks unique vehicle IDs, real-time GPS coordinates, and instantaneous velocity.
* **Collision Awareness:** Monitors the global map and broadcasts emergency directives (speed limits) when accidents are reported.

### 2. The Smart Vehicle (Client)
* **Kinematic Simulation:** Calculates real-time movement using velocity vectors and discrete-time integration.
* **Non-Blocking Telemetry:** Handles concurrent user input (keyboard) and server commands (socket) while maintaining a steady movement heartbeat.

---

## ⚙️ Technical Implementation

### 🔄 I/O Multiplexing with `select()`
To avoid the resource consumption of "one thread per client," this project implements the `select()` system call. This allows the application to monitor multiple file descriptors (network socket and `stdin`) simultaneously.


// Core event loop structure
FD_ZERO(&read_fds);
FD_SET(socket_fd, &read_fds);
FD_SET(STDIN_FILENO, &read_fds);

// Wait for activity or 1-second simulation pulse
int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

```

### 📐 Movement Physics Simulation

The client simulates vehicle displacement using a discrete-time approach triggered by the `select` timeout:

$$ \Delta d = v \times \Delta t $$
$$ Lat_{new} = Lat_{old} + (\Delta d \times \cos(\theta)) $$
$$ Long_{new} = Long_{old} + (\Delta d \times \sin(\theta)) $$

---

## 📡 Communication Protocol

Comunicația is handled via a lightweight, pipe-delimited string protocol for fast, low-overhead parsing in C.

| Direction | Format | Purpose |
| --- | --- | --- |
| **Client → Server** | `ID | LAT |
| **Server → Client** | `CMD | LIMIT |
| **Client → Server** | `EVENT | ACCIDENT |

### Data Integrity & Endianness

The project ensures cross-platform compatibility by converting multi-byte data (like port numbers) using:

* `htons()` / `ntohs()` (Host to Network Short)
* `htonl()` / `ntohl()` (Host to Network Long)

---

## 🛡️ Autonomous Safety Logic

### 🚫 Speed Governor (Auto-Pilot)

The client includes an autonomous safety callback. When a `WRN` (Warning) packet is received from the Authority:

1. It parses the new `speed_limit`.
2. If `current_speed > speed_limit`, the vehicle logic **overwrites** the local speed variable to enforce a safe deceleration.
3. The event is logged to the terminal to inform the operator.

### 🗺️ Boundary Recovery

If a vehicle hits the edge of the simulated environment (Status: `UNKNOWN`):

* **Bumper Protocol:** The vehicle reverses its direction vector ( turn).
* **State Rollback:** Reverts to the last known valid GPS coordinate to maintain simulation stability.

---

## 💻 Installation & Usage

### Building

Compilați fișierele folosind `gcc` pe un mediu Linux:

```bash
# Compile Server
gcc server.c -o server -Wall

# Compile Client
gcc client.c -o client -Wall

```

### Execution

1. **Start Server:** `./server 8080`
2. **Launch Vehicle:** `./client 127.0.0.1 8080`
3. **Initialize:** Type `set 44.43 26.10 60` in the client terminal to start movement.

---

## 🔍 Technical Challenges & Solutions

* **Challenge:** CPU overhead from busy-waiting.
* **Solution:** Switched to `select()` with a 1-second `timeval`, reducing CPU usage to nearly 0% during idle states.


* **Challenge:** "Address already in use" errors.
* **Solution:** Implemented `setsockopt` with `SO_REUSEADDR` to allow immediate port recycling.


* **Challenge:** Architecture differences in data representation.
* **Solution:** Rigorous use of `uint32_t` types and network byte order functions to ensure compatibility.



---

*Project developed as a study of Linux Systems Programming and Network Protocols.*

```

***

**Would you like me to ... add a `Makefile` script block to this documentation to make the build process easier for users?**

```
