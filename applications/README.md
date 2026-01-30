LATTICE is implemented with Darknet: A DNN Framework.


Mount and integrate PM 
=======================

* Persistent memory namespace needs to be configured in fs-dax mode.
* Mount the namespace in a dax enabled file system(Ext4 dax, xfs dax etc).
for more info: 
https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/7/html/storage_administration_guide/configuring-persistent-memory-for-file-system-direct-access-dax

* FM_FILE_NAME_PREFIX stores the path where LibVPM container file will be created and checkpointed data wil reside.
update FM_FILE_NAME_PREFIX value in  LATTICE/applications/darknet/src/classifier.c and LATTICE/src/settings.h


Compile LATTICE and update LATTICE library Path in darknet
=========================================================

* With the updated FM_FILE_NAME_PREFIX, compile LATTICE. an archive file libvpm.a will be created in LATTICE/build folder.
* Update applications/darknet/CMakeLists.txt's LIBPM_STATIC value with the absolute path of created libvpm.a
    set(LIBPM_STATIC $Absoulte path of the libvpm.a$ )


Configure checkpoint type and checkpoint frequency
=========================================================

* Checkpoint type(File IO/ LATTICE), checkpoint frequency needs to be configured before building darknet.

* To use LibVPM for checkpointing uncomment "#define LATTICE" in darknet/src/classifier.c. If commented
 File IO will be used for checkpointing.

 * "#define CP_FREQ 8" sets the checkpoint frequency in  darknet/src/classifier.c. Set CP_FREQ to different 
 values for checkpointing in different frequencies.

 * If CP_BACKGROUND is defined in darknet/src/parser.c, checkpoint operation will take place in background thread.
 Otherwise checkpoint will take place synchronously.


 Compile Darknet
=========================================================

* To compile Darknet in debug mode from applications/darknet :
    fuser -cu /local/mnt/
* To compile Darknet in release mode:
    "cd build_release  && cmake    .. && cmake --build . --target install --parallel 8 &&  cd .."



Run Darknet classifier
=========================================================

* To run darknet classifier for particular network config and training data:

 ./darknet classifier train data/mnist.data cfg/mnist.cfg (Darknet reference classifer with mnist data)

 ./darknet classifier train data/cifar/cifar.data cfg/cifar.cfg (Darknet reference classifer with CIFAR data)
 
