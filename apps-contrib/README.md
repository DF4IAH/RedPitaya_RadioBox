# Contributed applications

## Application list:
- bode_plotter
- lcr_meter
- lti
- radiobox
- scope+istsensor
- teslameter


## Application structure

	Bode_plotter, lcr_meter, lti, radiobox, scope+istsensor and teslameter
	have all a similar structure

|  path                           | contents
|---------------------------------|---------
| `apps-contrib/Makefile`               | Main Makefile used to build all applications listed here
| `apps-contrib/app_name/index.html` | Main client GUI file. It is used for the graphical view of the application in the web-browser
| `apps-contrib/app_name/Makefile`    | Application Makefile: mainly used to build src into controller.so
| `apps-contrib/app_name/info`         | Application meta-data in the application list of a red pitaya
| `apps-contrib/app_name/src`          | Main source directory, most of C code resides here
| `apps-contrib/app_name/fpga.conf`  | File containing the fpga.bit file location for each specific application
| `apps-contrib/app_name/doc`          | Documentation directory


# Build process

Please follow the instructions in the [README.md](../apps-free/README.md) file in the apps-free directory
