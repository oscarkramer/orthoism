![orthoism](orthoism.png?raw=true "Title")
# orthoism
Brute-force orthorectification using OSSIM. Unlike OSSIM's ubiquitous application `ossim-chipper --op ortho`, this application will perform a map->world->image projection for _every_ output pixel. The ossim-chipper app performs a dynamic grid interpolation to reduce the number of expensive projections performed and thus greatly speed up orthorectification.

### How to Build
Define an environment variable, `OSSIM_INSTALL_PREFIX`, pointing to the install directory that will contain OSSIM. This can be simply `/usr` but you'll need root priveledges when doing the `make install`.

Fetch [OSSIM](https://github.com/ossimlabs/ossim) and build the ossim library as explained in the readme. Make sure to perform a `make install` before proceeding.

Check the ossim build by running `ossim-info` (assuming your executable path is set correctly).

Then build orthoism application:
```
> cd orthoism
> mkdir build
> cd build
> cmake ..
> make
```
Alternatively you can import the directory into CLion or other IDE and build from there, assuming it knows how to read cmake files.

