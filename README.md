# 🚗 Smart Traffic Management System (TMS)

> **Monitorizarea și Controlul Autonom al Traficului prin Sockets TCP (Linux/C)**

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](https://en.cppreference.com/w/c)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)](https://www.kernel.org/)
[![Protocol](https://img.shields.io/badge/Protocol-TCP%2FIP-orange.svg)](https://datatracker.ietf.org/doc/html/rfc793)

Acest proiect implementează o arhitectură de rețea performantă menită să gestioneze fluxul de date între vehicule inteligente și o unitate centrală de control. Accentul este pus pe eficiența resurselor, utilizând un singur fir de execuție pentru sarcini multiple prin **multiplexare I/O**.

---

## 🏗️ Arhitectura Sistemului

Sistemul urmează modelul **Client-Server asimetric**, utilizând stiva de protocoale **TCP/IP**.



### 1. Nivelul Transport (TCP)
S-a ales protocolul **TCP (`SOCK_STREAM`)** în detrimentul UDP pentru a garanta:
* **Fiabilitatea:** Coordonatele GPS și alertele de accident nu pot fi pierdute sau corupte.
* **Ordinea datelor:** Mesajele de control (ex: limite de viteză) trebuie procesate exact în ordinea emiterii.

### 2. Protocol de Comunicație
Comunicația utilizează string-uri structurate (JSON-like) pentru parsing ușor:
* **Client → Server:** Telemetrie (`ID`, `Latitudine/Longitudine`, `Viteză`).
* **Server → Client:** Actualizări de hartă, limite de viteză și alerte critice.

---

## ⚙️ Mecanisme Core de Implementare

### 🔄 Multiplexarea I/O cu `select()`
Componenta centrală a clientului este bucla de evenimente care utilizează `select()`. Aceasta permite aplicației să fie **non-blocking** fără a utiliza `pthreads`.

* **Monitorizarea FD (File Descriptors):** Se urmăresc simultan `stdin` (tastatura) și socket-ul de rețea.
* **Logica de Simulare:** La fiecare timeout, se calculează deplasarea vehiculului:

$$distanta = viteza \times timp\_scurs$$
$$noua\_pozitie = pozitie\_veche + (\Delta \times directie)$$



### 🛠️ Parsing Manual și Manipularea String-urilor
Pentru a evita dependențele externe, parsing-ul se face prin pointeri și funcții din `string.h`:
* **Endianness:** Utilizarea `htons()` și `htonl()` pentru **Network Byte Order** (Big Endian), asigurând compatibilitatea între arhitecturi diferite.

---

## 🚀 Funcționalități de Siguranță

### 🛡️ Sistemul de Auto-Pilot (Speed Limiter)
Clientul conține un mecanism de feedback automat. La primirea unui mesaj `ALERT`:
1.  Extrage noua limită de viteză.
2.  Dacă `current_speed > speed_limit`, software-ul **suprascrie** variabila de viteză, simulând o frânare de urgență.

### 🗺️ Gestiunea Limitelor de Hartă (Boundary Check)
Dacă vehiculul iese din zona monitorizată (status `UNKNOWN`):
* Inversare direcție: `direction *= -1`.
* Revenire la ultimele coordonate valide: `prev_lat`, `prev_long`.

---

## 💻 Specificații de Utilizare

### 🛠️ Compilare
```bash
# Compilare Server
gcc server.c -o server -Wall

# Compilare Client
gcc client.c -o client -Wall
