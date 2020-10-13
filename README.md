# File Explorer

A simple File Explorer in written in C++. Works in Linux and Mac environments.

## Features

- Works in normal mode and command mode.
- Normal mode acts as a simple explorer where it is possible to view file/directory details and navigate between them.
- Command mode offers basic commands like file/folder creation, deletion, search, move, copy and rename. Also possible to switch to a different path.

## Usage

### Normal mode

- `Up arrow key` - Browse up to select item.
- `Down arrow key` - Browse down to select item.
- `Left arrow key` - Go back to prevously visited directory.
- `Right arrow key` - Go forward to prevously visited directory.
- `Enter` - Open directory/file.
- `Backspace` - Up one level.
- `H` or `h` - Go to `HOME`
- `K` or `k` - Scroll up by `SCROLL_CONSTANT` lines.
- `L` or `l` - Scroll down by `SCROLL_CONSTANT` lines.
- `:` - Enter command mode.
- `Q` or `q` - Quit application.

### Command mode

- `Enter` - Execute command.
- `ESC` Switch back to normal mode.

## Assumptions

- Terminal used is compatiable with VT100 codes.
- Filename along with list of properties does not overflow to next line.
- All commands issued in command mode are less than 4096 characters.
- `HOME` directory is determined to be the folder where the application is launched.
- Any path preceded by `~` is assumed to be absolute, otherwise assumed to be relative to current directory.
