`paxd-libre` is a daemon for automatically maintaining PaX exceptions of free software. It re-applies
exceptions whenever an executable is created / replaced. It also applies all of
the exceptions at start-up and when the configuration file is modified.

Since `paxd-libre` watches the parent directory chain for each executable, it has no
problem dealing with the creation of an executable in a directory that did not
exist when it was started. It works fine with all package managers it has been
tried with.

The sample `paxd-libre.conf` is targeted at Parabola GNU/Linux-libre, and the expectation is that
maintainers / users of other distributions will maintain a modified version
downstream.
