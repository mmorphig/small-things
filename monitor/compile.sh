# Monitor for a remote minecraft server, uses ssh to get everything

# scrolling any other window than the logs does not work right now, I'll fix it later, probably
# Be warned that this will somewhat flood any journal for ssh, it gets the logs using scp a lot and gets the system stats with ssh a lot

mkdir ../build

gcc -g -o ../build/main.o -c main.c -O3 
gcc -g -o monitor.x86_64 ../build/main.o -lncurses -lm
