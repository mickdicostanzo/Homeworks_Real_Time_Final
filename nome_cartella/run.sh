#!/bin/bash
echo "Pulizia code..."
rm -f /dev/mqueue/print_q
rm -f /dev/mqueue/mse_q
rm -f /dev/mqueue/watchdog_queue

# Avvia store
./store &
STORE_PID=$!

# Avvia filtro (con eventuali opzioni: -s -n -f -m/-b/-g)
./filter "$@" &
FILTER_PID=$!

# Avvia watchdog
./watchdog &
WATCHDOG_PID=$!

# Funzione per kill di tutti i processi
kill_processes() {
    kill "$STORE_PID" 2>/dev/null
    kill "$FILTER_PID" 2>/dev/null
    kill "$WATCHDOG_PID" 2>/dev/null
}

# Trap SIGINT (Ctrl+C)
trap kill_processes SIGINT

echo "Press 'q' to exit: "
read input

if [ "$input" == "q" ]; then
    kill_processes
    echo "" > signal.txt
fi
