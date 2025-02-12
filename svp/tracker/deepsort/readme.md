### Deepsort Cpp代码来源

https://github.com/shaoshengsong/DeepSORT

#### DeepSort流程

1.Detect获取目标框
2.把目标框里面的YUV图像进行resizeto(64*128)，通过NPU提取特征值
3.把特征值放到Deepsort里面进行Track
4.Deepsort track 获取对应的trackid
