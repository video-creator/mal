script_dir=$(cd `dirname $0`; pwd)
IOS_TOOLCHAIN=${script_dir}/ios-cmake/ios.toolchain.cmake
BUILD_DIR=${script_dir}/build
rm -rf $BUILD_DIR
if [[ ! -d $BUILD_DIR ]];then
    mkdir $BUILD_DIR
fi
echo "buid dir is $BUILD_DIR"
# rm -rf $BUILD_DIR



function build_ios() {
    release_mode=$1
    if [[ -z $release_mode ]];then
        release_mode="Release"
    fi
    plat=OS64COMBINED
    CMD="cmake  ${script_dir}/../CMakeLists.txt -B '$BUILD_DIR'
		-DTBB_TEST=OFF 
		-DCMAKE_BUILD_TYPE=$release_mode
		-DENABLE_BITCODE=1 
        -DBUILD_DIR=$BUILD_DIR
		-DENABLE_ARC=1 "
    CMD="$CMD  -DPLATFORM=MAC "
    CMD="$CMD && cmake --build "$BUILD_DIR" --config $release_mode"
    eval $CMD
    if [ $? -ne 0 ];then
            echo "build error!!!"
            exit 1
    fi
    pushd $BUILD_DIR
    cmake --install . --config $release_mode || exit 1
    popd
    if [ $? -ne 0 ];then
        echo "ERROR!!! MAC build faild!!!"
        exit 1
    else
        echo "MAC build success!!!"
    fi

}

function build_mac() {
    build_ios Debug || exit 1
    # build_ios mac Release
}


PLATFORM_TMP=`uname -s`
if [[ $PLATFORM_TMP == "Darwin" ]];then
    echo "build platform: mac"
    build_mac
else
    echo "ERROR!!! Current system not MAC!!"
    exit 1
fi




