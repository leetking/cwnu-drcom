# cwnu-drcom
针对西华师范大学做的一个校园网登录客户端，首要目标是为linux用户提供便利。

## 使用
直接make就好了

1. **登录**
 修改`drcomrc.example`里面的配置为自己的，并重命名为`drcomrc`放到当前目录下。
 然后输入
 ```bash
 sudo ./drcom
 ```
 就可以实现登录了。
 你也可以赋予drcom一个S权限并修改所有者为root
 ```bash
 sudo chown root:root drcom; sudo chmod +s drcom
 ```
 以后登录就直接`./drcom`就可以了

2. **注销**
 ```bash
 sudo ./drcom -l
 ```

3. **重新登录**
 ```bash
 sudo ./drocm -r
 ```
4. **记录登录日志**
 ```bash
 sudo ./drcom -d
 ```

5. **帮助**
 ```bash
 ./drcom -h
 ```

## 进度
- [x] 实现了eap协议，并学校实现eap层的自定义心跳。
- [x] 自动选择网卡登录

## TODO
- [ ] 实现drcom协议

## 下载
见上方[^](https://github.com/leetking/cwnu-drcom/releases)的`release`页面

## 自己编译？
想自定义或自己编译？
详情请右转[->](https://github.com/leetking/cwnu-drcom/wiki)见`wiki`页面的[如何编译](https://github.com/leetking/cwnu-drcom/wiki/HOW-TO-BUILD)
