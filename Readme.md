#### YOlOV8+deepsort SS928 3403 demo完整

通过文件作为视频输入，实时输出目标框到VO(HDMI),并且通过VGS把对应的框绘制到Frame上，通过LVGL把对应TrackID绘制在Framebuffer上

### 一、编译

依赖ss928交叉编译工具，和Cmake

```
./build.sh
```


### 二、运行程序

复制整个output到板端，确保板端ko正常加载

```
cd output
./rudemo.sh 5
```


### 三、效果如下

<video id="video" controls="" preload="none" poster="封面">
      <source id="mp4" src="doc/test.mp4" type="video/mp4">
</video>


###  四、专栏相关

哔哩哔哩
<https://www.bilibili.com/video/BV1bH4y1u7nv/>

csdn
<https://blog.csdn.net/apchy_ll/category_12628981.html?spm=1001.2014.3001.5482>

###  五、其它
创造不易，谢谢大家支持
<img src="doc/other/aipashhandemumu.png" alt="alt text" title="谢谢大家" style="max-width:100px;max-height:100px;">

如果需要项目合作，请添加下面微信
<img src="doc/other/wx.png" alt="alt text" title="谢谢大家" style="max-width:100px;max-height:100px;">
