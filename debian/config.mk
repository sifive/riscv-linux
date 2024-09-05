arch		= riscv64
human_arch	= RISC-V
build_arch	= riscv
defconfig	= defconfig
build_image	= Image
kernel_file	= arch/$(build_arch)/boot/Image
install_file	= vmlinuz

supported	= ESWIN EIC7700
target		= Geared toward desktop and server systems.
desc		= the HiFive Premier P550
bootloader	= grub-pc [amd64] | grub-efi-amd64 [amd64] | grub-efi-ia32 [amd64] | grub [amd64] | lilo [amd64] | flash-kernel [armhf arm64] | grub-efi-arm64 [arm64] | grub-efi-arm [armhf] | grub-ieee1275 [ppc64el]

gcc		= gcc-13
vdso		= vdso_install
no_dumpfile	= true

do_libc_dev_package	= false
do_metas		= true
do_tools_usbip		= true
do_tools_cpupower	= true
do_tools_perf		= true
do_tools_perf_jvmti	= true
do_tools_perf_python	= true
do_tools_bpftool	= true
do_tools_rtla		= false
do_dtbs			= true
