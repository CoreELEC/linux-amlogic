#! /bin/bash

export CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

make ARCH=arm meson64_a32_defconfig
make ARCH=arm  -j16 uImage  || echo "Compile Image Fail !!"
make ARCH=arm txl_t962_p321.dtb || echo "Compile dtb Fail !!"
make ARCH=arm sc2_pxp.dtb || echo "Compile dtb Fail !!"
make ARCH=arm sc2_s905x4_ah212.dtb || echo "Compile dtb Fail!!"
make ARCH=arm sc2_s905x4_ah219.dtb || echo "Compile dtb Fail!!"
make ARCH=arm tl1_t962x2_t309.dtb
make ARCH=arm  tl1_t962x2_x301_1g_drm.dtb
make ARCH=arm tl1_t962x2_x301_1g.dtb
make ARCH=arm tl1_t962x2_x301_2g_drm.dtb
make ARCH=arm tl1_t962x2_x301_2g.dtb
make ARCH=arm txlx_t962e_r321_buildroot.dtb
make ARCH=arm txlx_t962e_r321.dtb
make ARCH=arm txlx_t962x_r311_1g.dtb
make ARCH=arm txlx_t962x_r311_2g.dtb
make ARCH=arm txlx_t962x_r311_720p.dtb
make ARCH=arm txlx_t962x_r314.dtb
