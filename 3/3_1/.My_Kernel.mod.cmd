savedcmd_My_Kernel.mod := printf '%s\n'   My_Kernel.o | awk '!x[$$0]++ { print("./"$$0) }' > My_Kernel.mod
