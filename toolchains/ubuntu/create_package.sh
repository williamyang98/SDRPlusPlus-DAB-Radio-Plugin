#!/bin/sh
build_dir="build-ubuntu/src"
output_dir="plugin_package"
rm -rf ${output_dir}
mkdir -p ${output_dir}

cp ${build_dir}/*.so ${output_dir}/
cp README.md ${output_dir}/
