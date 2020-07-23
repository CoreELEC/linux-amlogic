.. Permission is granted to copy, distribute and/or modify this
.. document under the terms of the GNU Free Documentation License,
.. Version 1.1 or any later version published by the Free Software
.. Foundation, with no Invariant Sections, no Front-Cover Texts
.. and no Back-Cover Texts. A copy of the license is included at
.. Documentation/media/uapi/fdl-appendix.rst.
..
.. TODO: replace it to GFDL-1.1-or-later WITH no-invariant-sections

.. dts:

####################
DVB dts config
####################

The Digital TV device configure by dts in arch/arm/boot/dts/amlogic/
or arch/arm64/boot/dts/amlogic/, it can configure the dmx/ca/dvr/net
device num. at same time configure the external demod according to
different hardware platform
 for example:
  dvb {
		compatible = "amlogic, dvb";
		dev_name = "dvb";
		status = "ok";

	// using dmxdev num, max num is 32
		dmxdev_num = <4>;

		/*single demod setting */
		/*ts will bind with dmx device*/
		 ts0_dmx = <0>;
		 ts1_dmx = <1>;
		/*set pass tse or not, default is 1*/
        tse_enable = <0>;

		 /*multi demod setting, such as maxlinear demod*/
        	/*attention: ts how to bind to dmx device
		   it should set sid in maxlinear driver
	         sid list:
        	 dmx0 sid:0x20
	         dmx1 sid:0x21
        	 ...
	         dmx31 sid:0x2F
        	 */
	    ts2_header_len = <4>;
       	ts2_header = <0x4b>;
    	ts2_sid_offset = <1>;
	   	ts2 = "serial-3wire";
	    ts2_control = <0x0>;

		fe0_mode = "external";
		fe0_demod = "cxd2856";
		fe0_i2c_adap_id = <&i2c3>;
		fe0_demod_i2c_addr = <0xD8>;
		fe0_ts = <0>;
		fe0_reset_value = <0>;
		fe0_reset_gpio = <&gpio GPIOZ_10 GPIO_ACTIVE_HIGH>;
		fe0_ant_poweron_value = <0>;
		fe0_ant_power_gpio = <&gpio GPIOH_5 GPIO_ACTIVE_HIGH>;

		pinctrl-names = "s3_ts0";
		pinctrl-0 = <&dvb_s_ts0_pins>;
	};

.. note::

    dmxdev_num:if no define this variable, default the dmxdev_num is 32. and sid from 32 to 63
            it should according to the project and hardware to config.
    fex_xxx: it define the demod hardware configure.
    pinctrl-xxx: it define the demod pin mask.
    tsx_xxx: it define the ts configure with fex.
