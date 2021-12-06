# aquacomputer_quadro-hwmon
*A hwmon Linux kernel driver for exposing sensors of the Aquacomputer Quadro fan controller.*

Supports reading temperatures and speed, power, voltage and current of the attached fans and speed of a flowmeter. Being a standard `hwmon` driver, it provides readings via `sysfs`, which are easily accessible through `lm-sensors` as usual:

```shell
[leo@manjaro ~]$ sensors
quadro-hid-3-3
Adapter: HID adapter
VCC:           12.12 V  
Fan1 voltage:  12.12 V  
Fan2 voltage:  12.12 V  
Fan3 voltage:  12.12 V  
Fan4 voltage:  12.12 V  
Flow speed:    581 RPM
Fan1 speed:    306 RPM
Fan2 speed:   1760 RPM
Fan3 speed:    305 RPM
Fan4 speed:    308 RPM
Temp1:         +24.6°C  
Temp2:         +20.1°C  
Temp3:         +25.8°C  
Temp4:         +24.6°C  
Fan1 power:    90.00 mW 
Fan2 power:     0.00 W  
Fan3 power:    60.00 mW 
Fan4 power:    60.00 mW 
Fan1 current:   8.00 mA 
Fan2 current:   0.00 A  
Fan3 current:   5.00 mA 
Fan4 current:   5.00 mA 
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
