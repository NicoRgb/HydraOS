# HydraOS

HydraOS is a small Hobby OS for x86_64 written in C.

## Building

You will need the following requirements:

- bash
- python3
- make
- gcc
- g++
- fdisk
- x86_64-elf-binutils
- x86_64-elf-gcc
- nasm
- grub2-common
- grub-pc-bin
- xorriso
- qemu-system-x86_64 (for emulation)

You can install them manually or alternatively using the provided script:

```bash
bash scripts/setup.sh
```

Once all dependencies are install, you can build HydraOS with:

```bash
bash scripts/build.sh
```

And run it using:

```bash
bash scripts/run.sh
```

## Debugging

In order to use GDB to debug the kernel use the provided gdb script:
```bash
gdb -x debug.gdb
```

## Screenshots

In the future there will be screenshots of the OS here.

## Contributing and Coding Style

Please follow these guidelines when contributing:

- Follow the coding style of the project.
- Make commits following the [Conventional Commits 1.0.0 specification](https://www.conventionalcommits.org/en/v1.0.0/#specification).

## Authors

- Nico Grundei (<ni.grundei@gmail.com>)

## License

HydraOS is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for details.
