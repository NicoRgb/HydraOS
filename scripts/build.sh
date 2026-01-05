#!/bin/bash

set -e

DIR="`dirname "${BASH_SOURCE[0]}"`"
cd "$DIR" || exit

# rm -rf /tmp/hydra_root | true

mkdir -p /tmp/hydra_root/
mkdir -p /tmp/hydra_root/boot/grub
mkdir -p /tmp/hydra_root/bin
mkdir -p /tmp/hydra_root/lib
mkdir -p /tmp/hydra_root/mod
mkdir -p /tmp/hydra_root/include
mkdir -p /tmp/hydra_root/resources

cp -r ../kernel/include/* /tmp/hydra_root/include/

for dir in ../modules/*/; do
    if [ -d "$dir" ]; then
        pushd $dir
            module=$(basename "$dir")
            echo "Compiling $module"
            make build/${module}_module.a ROOT=/tmp/hydra_root/
            cp build/${module}_module.a /tmp/hydra_root/mod/${module}_module.a
        popd
    fi
done

pushd ../kernel
    echo "Compiling Kernel"
    make build/kernel.elf ROOT=/tmp/hydra_root/
    cp build/kernel.elf /tmp/hydra_root/boot/hydrakernel
popd

for dir in ../libs/*/; do
    if [ -d "$dir" ]; then
        pushd $dir
            lib=$(basename "$dir")
            echo "Compiling $lib"
            make build/$lib.a
            cp build/$lib.a /tmp/hydra_root/lib/$lib.a
            cp -r include/* /tmp/hydra_root/include
        popd
    fi
done

for dir in ../apps/*/; do
    if [ -d "$dir" ]; then
        pushd $dir
            app=$(basename "$dir")
            echo "Compiling $app"
            make build/$app ROOT=/tmp/hydra_root/
            cp build/$app /tmp/hydra_root/bin/$app

            if [ -d "resources" ]; then
                cp -r resources/* /tmp/hydra_root/resources
            fi
        popd
    fi
done

cat > /tmp/hydra_root/boot/grub/grub.cfg << EOF
set timeout=0
set default=0

menuentry "HydraOS" {
    multiboot2 /boot/hydrakernel klog=4660:4369
    boot
}
EOF

dd if=/dev/zero of=../hydraos.img bs=1M count=1024
fdisk ../hydraos.img << EOF
o
n
p
1


a
w
EOF

sudo losetup /dev/loop0 ../hydraos.img
sudo losetup /dev/loop1 ../hydraos.img -o 1048576

sudo mkdosfs -F32 -f 2 /dev/loop1
sudo mount /dev/loop1 /mnt
sudo cp -rf /tmp/hydra_root/* /mnt | true

sudo grub-install --target=i386-pc --root-directory=/mnt --no-floppy --modules="normal part_msdos multiboot2" /dev/loop0

sudo umount /mnt
sudo losetup -d /dev/loop0
sudo losetup -d /dev/loop1
