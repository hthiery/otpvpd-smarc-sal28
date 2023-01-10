# Restore the u-boot environment

![Build Status](https://github.com/kontron/otpvpd-smarc-sal28/workflows/build/badge.svg)

The on-board VPD EEPROM isn't write protected. In fact, it cannot be write
protected because the boot counter is stored there. If you accidentially
deleted your u-boot environment and you don't have a valid VPD content, the
serial number and the base MAC address is still stored in the OTP region of
the SPI flash. You can use this tool to read the OTP data.

## How to compile it

    $ gcc -Wall -std=c99 -o otpvpd otpvpd.c

## Sample scripts

You can find a sample script `restore-u-boot-variables` in the `contrib`
directory. This will use the u-boot-envtools to set the variables.

> :warning: The u-boot-envtools prior to version 2020.10 will always try to
> lock the environment sector. You might not be able to modify it within
> u-boot anymore.
