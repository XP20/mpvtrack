# mpvtrack
Plays videos with `mpv` in subdirs, ordering paths alphanumerically.

Stores `progress.txt` in current dir, and resumes where you left off.

## Installation
```sh
git clone https://github.com/xp20/mpvtrack.git
cd mpvtrack
make && make install
```

## Usage
Run `mpvtrack` in a parent folder of media (`MAX_DEPTH 3`).

When you run `mpvtrack`: it will try to read`./progress.txt`, parsing tracks and resuming playback.
Or creating and populating `./progress.txt` with discovered tracks, starting playback.

## Limitations
- `mpvtrack` does not forward `mpv`'s stdin/stdout, but its IPC socket is available.

## License
[MIT](LICENSE)
