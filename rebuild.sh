#!/bin/sh
cd /home/tengjunwan/Project/demo/ss928_yolov8n-master-20240815/ss928_yolov8n
if [ -d "build" ]; then
    echo "building folder already exists. deleting it ..."
    rm -rf build
fi

rm -rf output

# create a new build folder
echo "create build folder"
mkdir build

cd build

echo "running cmake and make ..."
cmake ..
make

echo "build completed"