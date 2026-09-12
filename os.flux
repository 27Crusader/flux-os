print "=== FLUX OS v0.1 ===";
print "Commands: 1 = Status, 2 = Help, 0 = Exit";

running = 1;
while (running == 1) {
    cmd = input;
    if (cmd == 1) {
        print "System Status: ONLINE";
    }
    if (cmd == 2) {
        print "Help: Enter numbers to execute commands.";
    }
    if (cmd == 0) {
        print "Shutting down Flux...";
        running = 0;
    }
}
