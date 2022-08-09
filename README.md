# aquacomputer_quadro-hwmon

## no further developement done here, quadro is supported by [aquacomputer_d5next](https://github.com/aleksamagicka/aquacomputer_d5next-hwmon)

*A hwmon Linux kernel driver for exposing sensors of the Aquacomputer Quadro fan controller.*

Supports reading temperatures and speed, power, voltage and current of attached fans. The speed of the flowmeter is reported in l/h and is only correct after configuration in aquasuite. Being a standard `hwmon` driver, it provides readings via `sysfs`, which are easily accessible through `lm-sensors` as usual:

```shell
[leo@manjaro ~]$ sensors
quadro-hid-3-3
Adapter: HID adapter
VCC:               12.12 V  
Fan1 voltage:      12.12 V  
Fan2 voltage:      12.12 V  
Fan3 voltage:      12.12 V  
Fan4 voltage:      12.12 V  
Flow speed [l/h]:   61 RPM
Fan1 speed:        423 RPM
Fan2 speed:       1758 RPM
Fan3 speed:        427 RPM
Fan4 speed:        428 RPM
Temp1:             +31.2°C  
Temp2:             +25.0°C  
Temp3:             +32.5°C  
Temp4:             +31.4°C  
Fan1 power:       140.00 mW 
Fan2 power:         0.00 W  
Fan3 power:       100.00 mW 
Fan4 power:        90.00 mW 
Fan1 current:      12.00 mA 
Fan2 current:       0.00 A  
Fan3 current:       9.00 mA 
Fan4 current:       8.00 mA
```

## Install

Go into the directory and simply run
```
make
```
and load the module by running (as a root)
```
insmod aquacomputer-quadro.ko
```

To remove the module simply run
```
rmmod aquacomputer-quadro.ko
```


based on [aquacomputer_d5next](https://github.com/aleksamagicka/aquacomputer_d5next-hwmon)
