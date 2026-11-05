# X și Zero - Multi-threaded TCP Client-Server

Acesta este un proiect de Rețele de Calculatoare complet funcțional, scris în **C**, folosind Socket-uri TCP și Multithreading.
Spre deosebire de un joc cu cuvinte, acest joc **nu necesită dicționare externe**, validarea se face algoritmic în server direct pe o matrice `3x3`.

## 🚀 Cum rulezi

1. **Compilare:**
   ```bash
   make
   ```

2. **Pornire Server (Terminal 1):**
   ```bash
   ./server 8888
   ```

3. **Pornire Jucători (Deschide 2 terminale separate):**
   ```bash
   ./client 127.0.0.1 8888
   ```
   * Introdu numele și alege camera `0`. Când ambii jucători sunt în aceeași cameră, jocul începe.
   * Tabla va fi desenată automat în terminal, iar căsuțele sunt numerotate de la `1` la `9`. 
   * Introdu cifra căsuței pentru a plasa un `X` sau un `O`.

## 🎨 Funcții speciale
* Matrice colorată ANSI (X e roșu, O e albastru).
* Validarea mutărilor pe server (nu poți pune peste alt jucător, nu poți trimite numere invalide).
* Verificarea victoriei și a remizei pe server.
