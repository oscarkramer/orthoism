# orthoism
Brute-force orthorectification using OSSIM

### How to Build
Fetch OSSIM [here](https://github.com/ossimlabs/ossim) and build the ossim library as explained there.

Then build orthoism application:
```
> cd orthoism
> mkdir build
> cd build
> cmake ..
> make
```
Alternatively you can import the directory into CLion or other IDE and build from there, assuming it knows how to read cmake files.

