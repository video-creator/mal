script_dir=$(cd `dirname $0`; pwd)

build_dir=${script_dir}/mac-build/build

if [ -d "$build_dir" ]; then
    make clean
else
    sh $script_dir/mac-build/compile-mac.sh
fi
cd $build_dir
make -j12