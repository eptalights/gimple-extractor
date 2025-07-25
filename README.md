# gimple-extractor
An [Eptalights](http://eptalights.com) Sophia GCC plugin for exporting GIMPLE IR instructions in JSON or MessagePack format.

### Supported GCC versions  

- `14.1.0` (not-supported)
- `13.3.0` (supported)
- `13.2.0` (supported)
- `12.4.0` (supported)
- `11.4.0` (supported)
- `10.5.0` (supported)
- `10.4.0` (supported)  

## With Docker: Building Plugin

##### Requirements  

[Docker](https://www.docker.com/) - Build, test, and deploy applications quickly. 

##### Build for specific GCC versions from the supported versions above  

```sh
git clone https://github.com/eptalights/sophia-extractor-cxx
cd /path/to/sophia-extractor-cxx
make docker-build-<supported-gcc-version>

# bin/sophia_extractor_gimple.so
```

## Without Docker: Building Plugin
##### Requirements  

[GCC](https://gcc.gnu.org/) - GNU Compiler Collection for front ends for C, C++, Objective-C, Fortran, Ada, Go, D and Modula-2.  
[GNU Make](https://www.gnu.org/software/make/) - a tool which controls the generation of executables and other non-source files of a program from the program's source files.  
[gcc plugin dev](#) - Install gcc-plugin-dev according to your platform and GCC version. 

##### Build for specific GCC versions from the supported versions above  

```sh
git clone https://github.com/eptalights/sophia-extractor-cxx
cd /path/to/sophia-extractor-cxx
make

# bin/sophia_extractor_gimple.so
```

## Usage

##### Compiling single source file.  

GCC to load the `sophia_extractor_gimple.so` plugin path specified in `fplugin` before compiling.  
Tell `sophia_extractor_gimple.so` where the source directory is using `fplugin-arg-sophia_extractor_gimple-source_path` to allow the extracted data to follow the same directory structure as the source directory.  
Data is extracted to the path specified in `fplugin-arg-sophia_extractor_gimple-extract_output_path`.
```sh
gcc -fplugin=/path/to/sophia_extractor_gimple.so \
	-fplugin-arg-sophia_extractor_gimple-source_path=/path/here \
	-fplugin-arg-sophia_extractor_gimple-output_path=/path/here \
	-c src/helloworld.cpp
```

There are only 2 supported data formats `(msgpack | json)` with the default data format `msgpack`.  
That can be changed using the flag `fplugin-arg-sophia_extractor_gimple-data_format`.
```sh
gcc -fplugin=/path/to/sophia_extractor_gimple.so \
	-fplugin-arg-sophia_extractor_gimple-source_path=/path/here \
	-fplugin-arg-sophia_extractor_gimple-output_path=/path/here \
	-fplugin-arg-sophia_extractor_gimple-data_format=json \
	-c src/helloworld.cpp
```

##### Compiling a code with a Makefile instead of a single source file.  

```sh
make CC="gcc -fplugin=/path/to/sophia_extractor_gimple.so -fplugin-arg-sophia_extractor_gimple-source_path=/path/here -fplugin-arg-sophia_extractor_gimple-output_path=/path/here"
```

##### Extracting Specific Paths in Large Projects

Sometimes projects can be huge, but we only want to extract a specific path of the project. We can achieve this by using `-fplugin-arg-sophia_extractor_gimple-source_path`.

```sh
g++ -fplugin=/sophia_extractor_gimple.so \
    -fplugin-arg-sophia_extractor_gimple-source_path=/path/to/selected/source/path \
    -fplugin-arg-sophia_extractor_gimple-output_path=/path/here \
    -c src/helloworld.cpp
```

#### Skipping Specific functions 

In the event of errors from particular functions, you can temporarily skip them to continue extraction, then raise a pull request to get them fixed.

```sh
gcc -fplugin=/path/to/sophia_extractor_gimple.so \
	-fplugin-arg-sophia_extractor_gimple-source_path=/path/here \
	-fplugin-arg-sophia_extractor_gimple-output_path=/path/here \
	-fplugin-arg-sophia_extractor_gimple-data_format=json \
	-fplugin-arg-sophia_extractor_gimple-skip_functions=function1,function2,... \
	-c src/helloworld.cpp
```
