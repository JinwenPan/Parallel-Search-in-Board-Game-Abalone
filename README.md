# Parallel Search in Board Game Abalone

[Abalone](https://en.wikipedia.org/wiki/Abalone_(board_game)) is a board game of class "2-Person Zero-Sum Game with Complete Information". Zero-sum means that an advantage of one player is exactly the disadvantage of his opponent. Complete information is given as there is no randomness or unkown information for the players in the game (such as dice or hidden cards).

## Technical Documentation

The network protocol is based on broadcasting board states on a virtual communication bus, which is identified by a host/port TCP address, on which one of the communicating game processes listens. Default is `localhost:23412`; two game processes started on one machine automatically can exchange data.

The default bus ID can be overridden by command line options `-p <port>` and `-h <hostname>`. If something goes wrong, debug information is printed via `-v` or `-vv` (more verbose). Every process first opens a listening socket on the port address provided, for others to be able to send a message to the process. If a port is already used, the next port number is tried. This way multiple processed can run on one machine.

Processes on a virtual communication bus know each other. Broadcasts actually are multiple messeages sent to all communication partners. For every message, a new TCP connection is opened, and afterwards closed again. This improves reliability if processes are crashing.

## Program `player`

This is a computer player using a given stone color. Colors are either `O` (default) or `X`, and can be specified as command line parameter. E.g. to play color `X`, use `player X`. A computer strength can be specified as another command line parameter, and is forwarded to the game play strategy used. However, one can think of strategies where no strength is needed, e.g. when playing against time. Whenever the player receives a board position where his color is about to draw, he starts "thinking", and after finding a move, he broadcasts the resulting board position.

## Program "start"

This starts programs by broadcasting a specified position, defaulting to the starting position of Abalone.  After that, it observes the communication bus and logs any received positions to the terminal. Other positions may be specified by a file which contains a position in ASCII art (using same style as used by "start" when logging positions to the terminal).

## Program "referee"

First, this does the same as "start", but uses two communication buses. Afterwards, it forwards game positions between these two busses, and checks whether subsequent game positions follow the game rules. If not, the referee complains and does not forward the position. Game positions are numbered, and contain a time. Both is corrected by the referee if needed. This way, players cannot cheat. However, using a referee is optional. The referee forwards a game position only if it has detected that there is another process running on the other bus. It blocks until a process is appearing.

## Compilation/Usage

Compile with `make`.

## Examples on One Machine

Without referee:
```bash
player O &
player X &
start
```

With referee:
```bash
player -p 3000 O &
player -p 4000 X &
referee -p 3000 -p 4000
```

The order of starting the programs does not matter. Similarly, if a program crashes or is killed, you can continue a game play by simply restarting the terminated player.


## Example with Players on Different Machines Using SSH Tunneling

One player should run on the local host `local` (`O`), and one (`X`) on host `remote`, which is only reachable via SSH. As we want "start" to observe the game on the local host, the remote must be able to open two connections to the local host. Thus, we need 3 tunnels: one from local to remote, and two from remote to local.

```bash
ssh -L 5000:localhost:5000 -R 5001:localhost:5001 -R 5002:localhost:5002 remote
remote> ./player -p 5000 O
local> ./player -p 5000 X &
local> ./start -p 5000
```

## Search Strategies

Multiple search strategies for deciding moves are supported and parallelized with OpenMP and MPI. The strategy can be specified for `player` by using the command line option `-s <stragegy>`. For a list of compiled-in strategies, run a player with `-h`.

The development process and experimental analysis are provided in the form of a lab report `/report/Abalone_Report.pdf` from a course at the Technical University of Munich. The framework for this software was provided by the instructor, and our main focus was on implementing search algorithms for decision-making.