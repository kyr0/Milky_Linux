# Milky Linux

## Live Audio DSP / Music Visualization

Milky Linux is a Linux-port of Milky - an audio visualization software that reminds of the 1998 released "Geiss" music visualizer (DOS/Windows 95, Windows 98).

It is purely rendered on the CPU. The whole graphics pipeline is coded without any 3rd-party dependency.

This project demonstrates how simplicity in software engineering can boost creativity and lead to a new/better understanding of both the hardware and software involved in all computers.

Software development mustn't be as hard and complicated as it appears to be these days.

### Build & Install

Run the following command to build this project:

```sh
sh > cmake . 
```
> Expected output:

```
-- The C compiler identification is GNU 11.4.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done
-- Generating done
-- Build files have been written to: /home/streamer/Code/Milky_Linux
```

Then run the compilation to create a binary executable file from the code:

```sh
sh > make 
```

