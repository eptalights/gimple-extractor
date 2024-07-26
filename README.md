# gimple-extractor
An [Eptalights](http://eptalights.com) GCC plugin to export GIMPLE IR instructions in JSON or MSGPACK format.

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
git clone https://github.com/finixbit/gimple-extractor
cd /path/to/gimple-extractor
make docker-build-<supported-gcc-version>

# bin/gimple_extractor.so
```

## Without Docker: Building Plugin
##### Requirements  

[GCC](https://gcc.gnu.org/) - GNU Compiler Collection for front ends for C, C++, Objective-C, Fortran, Ada, Go, D and Modula-2.  
[GNU Make](https://www.gnu.org/software/make/) - a tool which controls the generation of executables and other non-source files of a program from the program's source files.  

##### Build for specific GCC versions from the supported versions above  

```sh
git clone https://github.com/finixbit/gimple-extractor
cd /path/to/gimple-extractor
make

# bin/gimple_extractor.so
```

## Usage

##### Compiling single source file.  

GCC to load the `gimple_extractor.so` plugin path specified in `fplugin` before compiling.  
Tell `gimple_extractor.so` where the source directory is using `fplugin-arg-gimple_extractor-source_path` to allow the extracted data to follow the same directory structure as the source directory.  
Data is extracted to the path specified in `fplugin-arg-gimple_extractor-extract_output_path`.
```sh
gcc -fplugin=/path/to/gimple_extractor.so \
	-fplugin-arg-gimple_extractor-source_path=/path/here \
	-fplugin-arg-gimple_extractor-output_path=/path/here \
	-c src/helloworld.cpp
```

There are only 2 supported data formats `(msgpack | json)` with the default data format `msgpack`.  
That can be changed using the flag `fplugin-arg-gimple_extractor-data_format`.
```sh
gcc -fplugin=/path/to/gimple_extractor.so \
	-fplugin-arg-gimple_extractor-source_path=/path/here \
	-fplugin-arg-gimple_extractor-output_path=/path/here \
	-fplugin-arg-gimple_extractor-data_format=json \
	-c src/helloworld.cpp
```

##### Compiling a code with a Makefile instead of a single source file.  

```sh
make CC="gcc -fplugin=/path/to/gimple_extractor.so -fplugin-arg-gimple_extractor-source_path=/path/here -fplugin-arg-gimple_extractor-output_path=/path/here"
```

##### Extracting Specific Paths in Large Projects

Sometimes projects can be huge, but we only want to extract a specific path of the project. We can achieve this by using `-fplugin-arg-gimple_extractor-source_path`.

```sh
g++ -fplugin=/gimple_extractor.so \
    -fplugin-arg-gimple_extractor-source_path=/path/to/selected/source/path \
    -fplugin-arg-gimple_extractor-output_path=/path/here \
    -c src/helloworld.cpp
```