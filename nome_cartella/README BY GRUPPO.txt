Real-Time Programming Homework 1
Signal Generation, Filtering and Storage using POSIX real-time extensions

MEMBRI DEL GRUPPO:
- Annunziata Giovanni              DE6000015
- Di Costanzo Michele Pio          DE6000001
- Di Palma Lorenzo                 N39001908 
- Zaccone Amedeo                   DE6000014 


Sono state svolte sia le parti obbligatorie che quelle facoltative, 
con l'implementazione aggiuntiva del filtro di Savitzky-Golay (selezionabile 
mediante flag '-g').



Flag Usage: [-s] [-n] [-f] [-m | -b | -g]

-s : Signal value Plot;
-n : Generator Signal + Noise Signal Plot;

-f: Filtered Signal Plot;

OPTIONS:

-m : MA Filter;
-b : Butterworth Filter;
-g : Savitzky-Golay Filter;

NOTA:
dopo l'aggiunta del flag '-f', avviene la selezione del filtro ([-m | -b | -g]). 
Per la selezione del tipo di filtro si considera sempre l'ultimo flag inserito.



Per eseguire (in ordine) e verificare il funzionamento della selezione mediante flag:

sudo python3 live_plot.py
sudo taskset -c 3 ./store
sudo taskset -c 3 ./watchdog
sudo taskset -c 3 ./filter

Alternativamente è possibiile fare run contemporanea mediante:
sudo python3 live_plot.py
sudo taskset -c 3 ./run.sh

Per la chiusura (Ctrl + C) si consiglia, per pulizia nel plotting, di seguire 
il seguente ordine:

- filter
- watchdog
- store
