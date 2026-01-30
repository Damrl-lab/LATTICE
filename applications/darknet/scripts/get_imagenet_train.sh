#!/bin/bash

# wget http://www.image-net.org/challenges/LSVRC/2012/nnoupb/ILSVRC2012_img_train.tar
mkdir -p ILSVRC2021_img_train
tar --force-local -xf imagenet21k_resized.tar.gz -C imagenet21k_resized

wd=`pwd`

for f in imagenet21k_resized/*.tar;
do
name=$(echo "$f" | cut -f 1 -d '.')
mkdir "${wd}/${name}"
tar --force-local -xf "${wd}/${f}" -C "${wd}/${name}"
done

find "${wd}/ILSVRC2012_img_train" -name \*.JPEG > imagenet1k.train.list

