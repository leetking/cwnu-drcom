# cwnu-drcom

## NOTICE
**自2017-5-15学校升级服务器后，目前这个版本不能用。提供了一个lua版本封装的[web认证](http://github.com/leetking/cwnu-drcom.lua)**

针对西华师范大学做的一个校园网登录客户端，首要目标是为linux用户提供便利。

_**[drcom-generic](https://github.com/drcoms/drcom-generic.git)已经有了C版本，虽然本校还是可以直接用这个，如果觉得不好用请使用mchome大神的[dogcom](https://github.com/mchome/dogcom.git)。**_

## 使用

### A. 符合POSIX的系统（GNU/LINUX UNIX OSX）
直接make就好了，详细编译情况见wiki[如何编译](https://github.com/leetking/cwnu-drcom/wiki/HOW-TO-BUILD)

1. **登录**

 修改`drcomrc.example`里面的配置为自己的，并重命名为`drcomrc`放到当前目录下。
 然后输入
 ```bash
 $ sudo ./drcom
 ```
 就可以实现登录了。
 你也可以赋予drcom一个S权限并修改所有者为root
 ```bash
 $ sudo chown root:root drcom; sudo chmod +s drcom
 ```
 以后登录就直接`./drcom`

2. **注销**
 ```bash
 $ sudo ./drcom -l
 ```

3. **重新登录**
 ```bash
 $ sudo ./drocm -r
 ```
4. **记录登录日志**
 ```bash
 $ sudo ./drcom -d
 ```

5. **帮助**
 ```bash
 $ ./drcom -h
 ```
 
### B. 路由器（OPENWRT系列）
去[下载](https://github.com/leetking/cwnu-drcom/releases/latest)ipk安装包，自行安装。然后进入路由器设置信息保存，**重启**路由器

## 进度
- [x] 实现了eap协议，并学校实现eap层的自定义心跳
- [x] 自动选择网卡登录
- [x] 按照本校drcom客户端版本`v3.7.3(u31)`完整实现。学校客户端v3.7.3能支持全校登录，那么这个版本也能全校使用:)
- [x] 实现`openwrt`上的`UI`界面

## TODO
- [ ] 实现icmp判断网络连接情况
- [ ] 实现dhcp协议
- [ ] 实现完整`v5.2.0(x)`版本的drcom协议


## 下载
见上方[^](https://github.com/leetking/cwnu-drcom/releases)的`release`页面

## 自己编译？
想自定义或自己编译？
详情请右转[->](https://github.com/leetking/cwnu-drcom/wiki)见`wiki`页面的[如何编译](https://github.com/leetking/cwnu-drcom/wiki/HOW-TO-BUILD)
